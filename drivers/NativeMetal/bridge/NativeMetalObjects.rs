// SPDX-License-Identifier: MIT
//! The bridge's Metal objects behind a C ABI (NativeMetalObjects.h): the metal2vulkan runtime's device, resources, states, queues,
//! command buffers and encoders as boxed handles for the Objective-C frontend. Raw Metal enumerations go through the runtime's own
//! checked conversions; a value it does not know fails the call (never a substitute). Failures leave their reason in a per-thread
//! string, and no call unwinds into C.

use metal2vulkan::bind::SamplerDescriptor;
use metal2vulkan::metal::state::{DepthStencilDescriptor, StencilDescriptor};
use metal2vulkan::library::{FunctionConstantValue, FunctionConstantValues};
use metal2vulkan::metal::{
    BlendFactor, BlendOperation, ColorWriteMask, CompareFunction, CullMode, DataType, IndexType, PixelFormat, PrimitiveTopologyClass,
    PrimitiveType, RenderStages, SamplerAddressMode, SamplerBorderColor, SamplerMinMagFilter, SamplerMipFilter, StencilOperation, TextureType,
    TextureUsage, TriangleFillMode, VertexFormat, VertexStepFunction, Winding,
};
use metal2vulkan::pipeline::{
    ColorAttachmentDescriptor, RenderPipelineDescriptor, StageSource, VertexAttributeDescriptor, VertexBufferLayoutDescriptor,
    VertexDescriptor,
};
use metal2vulkan_runtime::{
    BlitCommandEncoder, BlitOption, Buffer, ClearColor, CommandBuffer, CommandQueue, ComputeCommandEncoder,
    ComputePipelineState, DepthStencilState, Device, DeviceOptions, Event, EventRef, Fence, Function, HostMemory, IoSurfacePlane, Library, LoadAction,
    MetalLevel, Origin, PipelineOptions, Region, RenderCommandEncoder, RenderPassDescriptor, RenderPipelineState, ResourceOptions,
    SamplerState, ScissorRect, SharedEvent, SharedEventListener, Size, StoreAction, Texture, TextureDescriptor, Viewport,
};
use std::cell::RefCell;
use std::ffi::{c_char, c_void, CStr};
use std::path::PathBuf;
use std::ptr::NonNull;
use std::time::Duration;

thread_local! {
    static LAST: RefCell<Vec<u8>> = const { RefCell::new(Vec::new()) };
}

fn set_error(why: String) {
    LAST.with(|last| {
        let mut last = last.borrow_mut();
        last.clear();
        last.extend(why.bytes().filter(|b| *b != 0));
        last.push(0);
    });
}

/// Runs `body`, turning an error or a panic into the per-thread reason and `fallback`.
fn guard<T>(fallback: T, body: impl FnOnce() -> Result<T, String>) -> T {
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(body)) {
        Ok(Ok(value)) => value,
        Ok(Err(why)) => {
            set_error(why);
            fallback
        }
        Err(_) => {
            set_error("panic in the Metal runtime".into());
            fallback
        }
    }
}

fn boxed<T>(value: T) -> *mut T {
    Box::into_raw(Box::new(value))
}

/// The object a handle points at.
fn get<'a, T>(handle: *const T) -> Result<&'a T, String> {
    // SAFETY: handles come from `boxed` and live until their release call (the header's contract).
    unsafe { handle.as_ref() }.ok_or_else(|| "null handle".to_string())
}

fn get_mut<'a, T>(handle: *mut T) -> Result<&'a mut T, String> {
    // SAFETY: as in `get`; an encoder is used by one thread at a time (Metal's rule).
    unsafe { handle.as_mut() }.ok_or_else(|| "null handle".to_string())
}

fn release<T>(handle: *mut T) {
    if !handle.is_null() {
        // SAFETY: from `boxed`, released once.
        drop(unsafe { Box::from_raw(handle) });
    }
}

fn copy_out(text: &str, out: *mut c_char, size: usize) -> usize {
    if !out.is_null() && size > 0 {
        let n = text.len().min(size - 1);
        // SAFETY: the caller's buffer holds `size` bytes.
        unsafe {
            std::ptr::copy_nonoverlapping(text.as_ptr(), out as *mut u8, n);
            *out.add(n) = 0;
        }
    }
    text.len()
}

fn raw<T: TryFrom<u32>>(value: u64, what: &str) -> Result<T, String>
where
    T::Error: std::fmt::Debug,
{
    u32::try_from(value).ok().and_then(|v| T::try_from(v).ok()).ok_or_else(|| format!("{what} {value} is not a Metal value"))
}

fn e<E: std::fmt::Debug>(what: &'static str) -> impl FnOnce(E) -> String {
    move |error| format!("{what}: {error:?}")
}

fn options(value: u64) -> Result<ResourceOptions, String> {
    ResourceOptions::from_raw(value).map_err(e("resource options"))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_last_error() -> *const c_char {
    LAST.with(|last| {
        let last = last.borrow();
        if last.is_empty() { c"".as_ptr() } else { last.as_ptr() as *const c_char }
    })
}

/// The runtime's kernel_function_constant_gate_fold fixture, assembled to AIR at build time.
static FUNCTION_CONSTANT_AIR: &[u8] = include_bytes!(env!("NMB_FC_AIR"));

/// # Safety
/// `length` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_fixture_air(length: *mut usize) -> *const c_void {
    // SAFETY: the caller's contract.
    if let Some(length) = unsafe { length.as_mut() } {
        *length = crate::FIXTURE_AIR.len();
    }
    crate::FIXTURE_AIR.as_ptr() as *const c_void
}

/// # Safety
/// `length` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_function_constant_air(length: *mut usize) -> *const c_void {
    // SAFETY: the caller's contract.
    if let Some(length) = unsafe { length.as_mut() } {
        *length = FUNCTION_CONSTANT_AIR.len();
    }
    FUNCTION_CONSTANT_AIR.as_ptr() as *const c_void
}

// ---- device ----

pub struct NmbDevice(Device);

/// # Safety
/// `loader` and `cache` are C strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_device_open(loader: *const c_char, cache: *const c_char) -> *mut NmbDevice {
    guard(std::ptr::null_mut(), || {
        if loader.is_null() || cache.is_null() {
            return Err("loader and cache are required".into());
        }
        // SAFETY: the caller's contract.
        let (loader, cache) = unsafe { (CStr::from_ptr(loader), CStr::from_ptr(cache)) };
        let mut options = DeviceOptions::default();
        options.loader = Some(PathBuf::from(loader.to_string_lossy().into_owned()));
        options.metal_level = Some(MetalLevel::NEWEST);
        options.shader_cache = metal2vulkan_runtime::ShaderCacheOptions::in_directory(cache.to_string_lossy().into_owned());
        Ok(boxed(NmbDevice(Device::system_default_with(&options).map_err(e("device"))?)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_device_release(device: *mut NmbDevice) {
    release(device)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_device_name(device: *const NmbDevice, out: *mut c_char, size: usize) -> usize {
    guard(0, || Ok(copy_out(get(device)?.0.name(), out, size)))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_device_max_buffer_length(device: *const NmbDevice) -> u64 {
    guard(0, || Ok(get(device)?.0.properties().max_buffer_length))
}

// ---- buffers ----

pub struct NmbBuffer(Buffer);

/// # Safety
/// `bytes` is null or holds `length` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_buffer_new(device: *mut NmbDevice, length: u64, options_word: u64, bytes: *const c_void) -> *mut NmbBuffer {
    guard(std::ptr::null_mut(), || {
        let device = &get(device)?.0;
        let options = options(options_word)?;
        let buffer = if bytes.is_null() {
            device.new_buffer(length, options)
        } else {
            // SAFETY: the caller's contract.
            let bytes = unsafe { std::slice::from_raw_parts(bytes as *const u8, length as usize) };
            device.new_buffer_with_bytes(bytes, options)
        };
        Ok(boxed(NmbBuffer(buffer.map_err(e("buffer"))?)))
    })
}

/// # Safety
/// `pointer` is page aligned, holds `length` bytes and stays mapped until the buffer is released.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_buffer_new_no_copy(device: *mut NmbDevice, pointer: *mut c_void, length: u64, options_word: u64)
                                                -> *mut NmbBuffer {
    guard(std::ptr::null_mut(), || {
        let device = &get(device)?.0;
        // SAFETY: the caller's contract.
        let memory = unsafe { HostMemory::new(NonNull::new(pointer as *mut u8).ok_or("null memory")?) };
        // SAFETY: as above.
        let buffer = unsafe { device.new_buffer_with_bytes_no_copy(memory, length, options(options_word)?, None) };
        Ok(boxed(NmbBuffer(buffer.map_err(e("no-copy buffer"))?)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_buffer_release(buffer: *mut NmbBuffer) {
    release(buffer)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_buffer_contents(buffer: *const NmbBuffer) -> *mut c_void {
    guard(std::ptr::null_mut(), || Ok(get(buffer)?.0.contents().map_or(std::ptr::null_mut(), |p| p.as_ptr() as *mut c_void)))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_buffer_length(buffer: *const NmbBuffer) -> u64 {
    guard(0, || Ok(get(buffer)?.0.length()))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_buffer_gpu_address(buffer: *const NmbBuffer) -> u64 {
    guard(0, || Ok(get(buffer)?.0.gpu_address().0))
}

// ---- textures ----

#[repr(C)]
pub struct NmbTextureDescriptor {
    pub kind: u64,
    pub format: u64,
    pub width: u64,
    pub height: u64,
    pub depth: u64,
    pub levels: u64,
    pub samples: u64,
    pub array_length: u64,
    pub usage: u64,
    pub options: u64,
}

pub struct NmbTexture(Texture);

fn texture_descriptor(d: *const NmbTextureDescriptor) -> Result<TextureDescriptor, String> {
    let d = get(d)?;
    let mut out = TextureDescriptor::default();
    out.texture_type = raw::<TextureType>(d.kind, "texture type")?;
    out.pixel_format = raw::<PixelFormat>(d.format, "pixel format")?;
    out.width = d.width;
    out.height = d.height;
    out.depth = d.depth;
    out.mipmap_level_count = d.levels;
    out.sample_count = d.samples;
    out.array_length = d.array_length;
    out.usage = raw::<TextureUsage>(d.usage, "texture usage")?;
    out.resource_options = options(d.options)?;
    Ok(out)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_texture_new(device: *mut NmbDevice, descriptor: *const NmbTextureDescriptor) -> *mut NmbTexture {
    guard(std::ptr::null_mut(), || {
        let descriptor = texture_descriptor(descriptor)?;
        Ok(boxed(NmbTexture(get(device)?.0.new_texture(&descriptor).map_err(e("texture"))?)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_texture_new_on_buffer(buffer: *mut NmbBuffer, descriptor: *const NmbTextureDescriptor, offset: u64,
                                            bytes_per_row: u64, iosurface: bool) -> *mut NmbTexture {
    guard(std::ptr::null_mut(), || {
        let descriptor = texture_descriptor(descriptor)?;
        let buffer = &get(buffer)?.0;
        let texture = if iosurface {
            buffer.new_texture_with_iosurface_plane(&descriptor, IoSurfacePlane::new(offset, bytes_per_row, false))
        } else {
            buffer.new_texture(&descriptor, offset, bytes_per_row)
        };
        Ok(boxed(NmbTexture(texture.map_err(e("texture over buffer"))?)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_texture_release(texture: *mut NmbTexture) {
    release(texture)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_texture_describe(texture: *const NmbTexture, out: *mut NmbTextureDescriptor) {
    guard((), || {
        let t = &get(texture)?.0;
        let out = get_mut(out)?;
        *out = NmbTextureDescriptor {
            kind: t.texture_type().as_raw() as u64,
            format: t.pixel_format().as_raw() as u64,
            width: t.width(),
            height: t.height(),
            depth: t.depth(),
            levels: t.mipmap_level_count(),
            samples: t.sample_count(),
            array_length: t.array_length(),
            usage: t.usage().as_raw() as u64,
            options: t.resource_options().raw(),
        };
        Ok(())
    })
}

fn region(r: *const u64) -> Result<Region, String> {
    if r.is_null() {
        return Err("null region".into());
    }
    // SAFETY: the header's six words.
    let r = unsafe { std::slice::from_raw_parts(r, 6) };
    Ok(Region::new_3d(r[0], r[1], r[2], r[3], r[4], r[5]))
}

fn triple(v: *const u64) -> Result<[u64; 3], String> {
    if v.is_null() {
        return Err("null origin or size".into());
    }
    // SAFETY: the header's three words.
    let v = unsafe { std::slice::from_raw_parts(v, 3) };
    Ok([v[0], v[1], v[2]])
}

/// # Safety
/// `bytes` holds `length` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_texture_replace(texture: *mut NmbTexture, region_words: *const u64, level: u64, slice: u64,
                                             bytes: *const c_void, bytes_per_row: u64, bytes_per_image: u64, length: u64) -> bool {
    guard(false, || {
        if bytes.is_null() {
            return Err("null bytes".into());
        }
        // SAFETY: the caller's contract.
        let bytes = unsafe { std::slice::from_raw_parts(bytes as *const u8, length as usize) };
        get(texture)?.0.replace_region(region(region_words)?, level, slice, bytes, bytes_per_row, bytes_per_image)
            .map_err(e("replace region"))?;
        Ok(true)
    })
}

/// # Safety
/// `bytes` holds `length` writable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_texture_get_bytes(texture: *const NmbTexture, bytes: *mut c_void, bytes_per_row: u64, bytes_per_image: u64,
                                               length: u64, region_words: *const u64, level: u64, slice: u64) -> bool {
    guard(false, || {
        if bytes.is_null() {
            return Err("null bytes".into());
        }
        // SAFETY: the caller's contract.
        let bytes = unsafe { std::slice::from_raw_parts_mut(bytes as *mut u8, length as usize) };
        get(texture)?.0.get_bytes(bytes, bytes_per_row, bytes_per_image, region(region_words)?, level, slice)
            .map_err(e("get bytes"))?;
        Ok(true)
    })
}

// ---- libraries and functions ----

pub struct NmbLibrary(Library, Vec<String>);
pub struct NmbFunction(Function);

/// # Safety
/// `bytes` holds `length` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_library_new(device: *mut NmbDevice, bytes: *const c_void, length: usize, air: bool) -> *mut NmbLibrary {
    guard(std::ptr::null_mut(), || {
        let device = &get(device)?.0;
        if bytes.is_null() || length == 0 {
            return Err("no library bytes".into());
        }
        // SAFETY: the caller's contract.
        let bytes = unsafe { std::slice::from_raw_parts(bytes as *const u8, length) };
        let library = if air {
            device.new_library_with_compile_library(metal2vulkan::library::Library::from_air(bytes).map_err(e("AIR"))?)
        } else {
            device.new_library_with_data(crate::air_slice(bytes).ok_or("metallib without an AIR64 slice")?).map_err(e("metallib"))?
        };
        let names = library.function_names();
        Ok(boxed(NmbLibrary(library, names)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_library_release(library: *mut NmbLibrary) {
    release(library)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_library_function_count(library: *const NmbLibrary) -> u32 {
    guard(0, || Ok(get(library)?.1.len() as u32))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_library_function_name(library: *const NmbLibrary, index: u32, out: *mut c_char, size: usize) -> usize {
    guard(0, || {
        let names = &get(library)?.1;
        Ok(copy_out(names.get(index as usize).ok_or("function index")?, out, size))
    })
}

/// # Safety
/// `name` is a C string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_function_new(library: *mut NmbLibrary, name: *const c_char) -> *mut NmbFunction {
    guard(std::ptr::null_mut(), || {
        if name.is_null() {
            return Err("null function name".into());
        }
        // SAFETY: the caller's contract.
        let name = unsafe { CStr::from_ptr(name) }.to_str().map_err(e("function name"))?;
        Ok(boxed(NmbFunction(get(library)?.0.new_function_with_name(name).map_err(e("function"))?)))
    })
}

#[repr(C)]
pub struct NmbConstant {
    pub kind: u64,
    pub index: u64,
    pub name: *const c_char,
    pub bytes: *const c_void,
    pub length: u64,
}

/// # Safety
/// `name` is a C string; `constants` holds `count` entries whose names are C strings (or null) and whose bytes hold `length` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_function_new_specialized(library: *mut NmbLibrary, name: *const c_char, constants: *const NmbConstant,
                                                      count: u32) -> *mut NmbFunction {
    guard(std::ptr::null_mut(), || {
        if name.is_null() || (constants.is_null() && count > 0) {
            return Err("null function name or constants".into());
        }
        // SAFETY: the caller's contract.
        let name = unsafe { CStr::from_ptr(name) }.to_str().map_err(e("function name"))?;
        let constants = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(constants, count as usize) } };
        let mut values = FunctionConstantValues::defaults();
        for c in constants {
            if c.bytes.is_null() {
                return Err("constant without bytes".into());
            }
            // SAFETY: the caller's contract.
            let bytes = unsafe { std::slice::from_raw_parts(c.bytes as *const u8, c.length as usize) };
            let value = FunctionConstantValue::new(raw::<DataType>(c.kind, "constant type")?, bytes.to_vec());
            if c.name.is_null() {
                values.by_index.insert(u32::try_from(c.index).map_err(e("constant index"))?, value);
            } else {
                // SAFETY: the caller's contract.
                let key = unsafe { CStr::from_ptr(c.name) }.to_str().map_err(e("constant name"))?;
                values.by_name.insert(key.to_string(), value);
            }
        }
        let function = get(library)?.0.new_function_with_name_constant_values(name, &values).map_err(e("specialized function"))?;
        Ok(boxed(NmbFunction(function)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_function_release(function: *mut NmbFunction) {
    release(function)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_function_type(function: *const NmbFunction) -> u64 {
    guard(0, || Ok(get(function)?.0.function_type().map_err(e("function type"))?.as_raw() as u64))
}

// ---- pipelines and states ----

pub struct NmbComputePipeline(ComputePipelineState);
pub struct NmbRenderPipeline(RenderPipelineState);
pub struct NmbSampler(SamplerState);
pub struct NmbDepthStencil(DepthStencilState);

#[unsafe(no_mangle)]
pub extern "C" fn nmb_compute_pipeline_new(device: *mut NmbDevice, function: *mut NmbFunction) -> *mut NmbComputePipeline {
    guard(std::ptr::null_mut(), || {
        let state = get(device)?.0.new_compute_pipeline_state_with_function(&get(function)?.0, &PipelineOptions::default());
        Ok(boxed(NmbComputePipeline(state.map_err(e("compute pipeline"))?)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_compute_pipeline_release(pipeline: *mut NmbComputePipeline) {
    release(pipeline)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_compute_pipeline_max_threads(pipeline: *const NmbComputePipeline) -> u64 {
    guard(0, || Ok(get(pipeline)?.0.max_total_threads_per_threadgroup() as u64))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_compute_pipeline_execution_width(pipeline: *const NmbComputePipeline) -> u64 {
    guard(0, || Ok(get(pipeline)?.0.thread_execution_width() as u64))
}

#[repr(C)]
pub struct NmbColorAttachment {
    pub format: u64,
    pub write_mask: u64,
    pub blending: u64,
    pub source_rgb: u64,
    pub destination_rgb: u64,
    pub rgb_operation: u64,
    pub source_alpha: u64,
    pub destination_alpha: u64,
    pub alpha_operation: u64,
}

#[repr(C)]
pub struct NmbVertexAttribute {
    pub format: u64,
    pub offset: u64,
    pub buffer: u64,
}

#[repr(C)]
pub struct NmbVertexLayout {
    pub stride: u64,
    pub step_function: u64,
    pub step_rate: u64,
}

#[repr(C)]
pub struct NmbRenderPipelineDescriptor {
    pub vertex: *mut NmbFunction,
    pub fragment: *mut NmbFunction,
    pub colors: [NmbColorAttachment; 8],
    pub depth_format: u64,
    pub stencil_format: u64,
    pub samples: u64,
    pub topology: u64,
    pub rasterization: u64,
    pub vertex_inputs: u64,
    pub attributes: [NmbVertexAttribute; 31],
    pub layouts: [NmbVertexLayout; 31],
}

fn optional_format(value: u64) -> Result<Option<PixelFormat>, String> {
    if value == 0 { Ok(None) } else { raw::<PixelFormat>(value, "pixel format").map(Some) }
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_pipeline_new(device: *mut NmbDevice, descriptor: *const NmbRenderPipelineDescriptor) -> *mut NmbRenderPipeline {
    guard(std::ptr::null_mut(), || {
        let device = &get(device)?.0;
        let d = get(descriptor)?;
        let mut out = RenderPipelineDescriptor::new(StageSource::Function(get(d.vertex)?.0.function_descriptor()));
        if !d.fragment.is_null() {
            out.fragment = Some(StageSource::Function(get(d.fragment)?.0.function_descriptor()));
        }
        for (slot, c) in out.color_attachments.iter_mut().zip(d.colors.iter()) {
            if c.format == 0 {
                continue;
            }
            let mut a = ColorAttachmentDescriptor::new(raw::<PixelFormat>(c.format, "color format")?);
            a.write_mask = raw::<ColorWriteMask>(c.write_mask, "write mask")?;
            a.blending_enabled = c.blending != 0;
            a.source_rgb_blend_factor = raw::<BlendFactor>(c.source_rgb, "blend factor")?;
            a.destination_rgb_blend_factor = raw::<BlendFactor>(c.destination_rgb, "blend factor")?;
            a.rgb_blend_operation = raw::<BlendOperation>(c.rgb_operation, "blend operation")?;
            a.source_alpha_blend_factor = raw::<BlendFactor>(c.source_alpha, "blend factor")?;
            a.destination_alpha_blend_factor = raw::<BlendFactor>(c.destination_alpha, "blend factor")?;
            a.alpha_blend_operation = raw::<BlendOperation>(c.alpha_operation, "blend operation")?;
            *slot = Some(a);
        }
        out.depth_format = optional_format(d.depth_format)?;
        out.stencil_format = optional_format(d.stencil_format)?;
        out.raster_sample_count = d.samples.max(1) as u32;
        out.input_primitive_topology = raw::<PrimitiveTopologyClass>(d.topology, "topology")?;
        out.rasterization_enabled = d.rasterization != 0;
        if d.vertex_inputs != 0 {
            let mut inputs = VertexDescriptor::new();
            for (i, a) in d.attributes.iter().enumerate() {
                if a.format == 0 {
                    continue;
                }
                inputs.attributes.insert(i as u32, VertexAttributeDescriptor::new(raw::<VertexFormat>(a.format, "vertex format")?,
                                                                                    a.offset as u32, a.buffer as u32));
                if let Some(l) = d.layouts.get(a.buffer as usize) {
                    let mut layout = VertexBufferLayoutDescriptor::new(Some(l.stride as u32));
                    layout.step_function = raw::<VertexStepFunction>(l.step_function, "step function")?;
                    layout.step_rate = l.step_rate as u32;
                    inputs.layouts.insert(a.buffer as u32, layout);
                }
            }
            out.vertex_descriptor = Some(inputs);
        }
        let state = device.new_render_pipeline_state(&out, &PipelineOptions::default()).map_err(e("render pipeline"))?;
        Ok(boxed(NmbRenderPipeline(state)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_pipeline_release(pipeline: *mut NmbRenderPipeline) {
    release(pipeline)
}

#[repr(C)]
pub struct NmbSamplerDescriptor {
    pub min_filter: u64,
    pub mag_filter: u64,
    pub mip_filter: u64,
    pub s_address: u64,
    pub t_address: u64,
    pub r_address: u64,
    pub border_color: u64,
    pub normalized: u64,
    pub max_anisotropy: u64,
    pub compare: u64,
    pub lod_average: u64,
    pub lod_min: f32,
    pub lod_max: f32,
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_sampler_new(device: *mut NmbDevice, descriptor: *const NmbSamplerDescriptor) -> *mut NmbSampler {
    guard(std::ptr::null_mut(), || {
        let d = get(descriptor)?;
        let mut out = SamplerDescriptor::new();
        out.min_filter = raw::<SamplerMinMagFilter>(d.min_filter, "min filter")?;
        out.mag_filter = raw::<SamplerMinMagFilter>(d.mag_filter, "mag filter")?;
        out.mip_filter = raw::<SamplerMipFilter>(d.mip_filter, "mip filter")?;
        out.s_address_mode = raw::<SamplerAddressMode>(d.s_address, "address mode")?;
        out.t_address_mode = raw::<SamplerAddressMode>(d.t_address, "address mode")?;
        out.r_address_mode = raw::<SamplerAddressMode>(d.r_address, "address mode")?;
        out.border_color = raw::<SamplerBorderColor>(d.border_color, "border color")?;
        out.normalized_coordinates = d.normalized != 0;
        out.max_anisotropy = d.max_anisotropy.max(1) as u32;
        out.compare_function = raw::<CompareFunction>(d.compare, "compare function")?;
        out.lod_average = d.lod_average != 0;
        out.lod_min_clamp = d.lod_min;
        out.lod_max_clamp = d.lod_max;
        Ok(boxed(NmbSampler(get(device)?.0.new_sampler_state(&out).map_err(e("sampler"))?)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_sampler_release(sampler: *mut NmbSampler) {
    release(sampler)
}

#[repr(C)]
pub struct NmbStencil {
    pub compare: u64,
    pub failure: u64,
    pub depth_failure: u64,
    pub pass: u64,
    pub read_mask: u64,
    pub write_mask: u64,
}

#[repr(C)]
pub struct NmbDepthStencilDescriptor {
    pub compare: u64,
    pub write: u64,
    pub front: NmbStencil,
    pub back: NmbStencil,
}

fn stencil(s: &NmbStencil) -> Result<StencilDescriptor, String> {
    let mut out = StencilDescriptor::default();
    out.stencil_compare_function = raw::<CompareFunction>(s.compare, "stencil compare")?;
    out.stencil_failure_operation = raw::<StencilOperation>(s.failure, "stencil operation")?;
    out.depth_failure_operation = raw::<StencilOperation>(s.depth_failure, "stencil operation")?;
    out.depth_stencil_pass_operation = raw::<StencilOperation>(s.pass, "stencil operation")?;
    out.read_mask = s.read_mask as u32;
    out.write_mask = s.write_mask as u32;
    Ok(out)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_depth_stencil_new(device: *mut NmbDevice, descriptor: *const NmbDepthStencilDescriptor) -> *mut NmbDepthStencil {
    guard(std::ptr::null_mut(), || {
        let d = get(descriptor)?;
        let mut out = DepthStencilDescriptor::default();
        out.depth_compare_function = raw::<CompareFunction>(d.compare, "depth compare")?;
        out.depth_write_enabled = d.write != 0;
        out.front_face_stencil = stencil(&d.front)?;
        out.back_face_stencil = stencil(&d.back)?;
        Ok(boxed(NmbDepthStencil(get(device)?.0.new_depth_stencil_state(&out).map_err(e("depth/stencil state"))?)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_depth_stencil_release(state: *mut NmbDepthStencil) {
    release(state)
}

// ---- queues and command buffers ----

pub struct NmbQueue(CommandQueue);
pub struct NmbCommandBuffer(CommandBuffer);

#[unsafe(no_mangle)]
pub extern "C" fn nmb_queue_new(device: *mut NmbDevice) -> *mut NmbQueue {
    guard(std::ptr::null_mut(), || Ok(boxed(NmbQueue(get(device)?.0.new_command_queue().map_err(e("queue"))?))))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_queue_release(queue: *mut NmbQueue) {
    release(queue)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_new(queue: *mut NmbQueue) -> *mut NmbCommandBuffer {
    guard(std::ptr::null_mut(), || Ok(boxed(NmbCommandBuffer(get(queue)?.0.command_buffer().map_err(e("command buffer"))?))))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_release(command_buffer: *mut NmbCommandBuffer) {
    release(command_buffer)
}

struct Context(*mut c_void);
// SAFETY: the context is the caller's, handed back on the runtime's thread as the header states.
unsafe impl Send for Context {}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_on_completed(command_buffer: *mut NmbCommandBuffer, handler: Option<extern "C" fn(*mut c_void)>,
                                                  context: *mut c_void) -> bool {
    guard(false, || {
        let handler = handler.ok_or("null handler")?;
        let context = Context(context);
        get(command_buffer)?.0.add_completed_handler(Box::new(move |_| {
            let context = context;
            handler(context.0)
        })).map_err(e("completed handler"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_commit(command_buffer: *mut NmbCommandBuffer) -> bool {
    guard(false, || {
        get(command_buffer)?.0.commit().map_err(e("commit"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_wait(command_buffer: *mut NmbCommandBuffer, timeout_ms: u64) -> i32 {
    guard(-1, || {
        let command_buffer = &get(command_buffer)?.0;
        if timeout_ms == 0 {
            command_buffer.wait_until_completed().map_err(e("wait"))?;
            return Ok(0);
        }
        Ok(if command_buffer.wait_until_completed_for(Duration::from_millis(timeout_ms)).is_ok() { 0 } else { 1 })
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_status(command_buffer: *const NmbCommandBuffer) -> u64 {
    guard(5, || {
        Ok(get(command_buffer)?.0.status().raw())
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_error(command_buffer: *const NmbCommandBuffer, out: *mut c_char, size: usize) -> usize {
    guard(0, || Ok(get(command_buffer)?.0.error_description().map_or(0, |text| copy_out(&text, out, size))))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_gpu_time(command_buffer: *const NmbCommandBuffer, end: bool) -> f64 {
    guard(0.0, || {
        let command_buffer = &get(command_buffer)?.0;
        let time = if end { command_buffer.gpu_end_time() } else { command_buffer.gpu_start_time() };
        time.map_err(e("GPU time"))
    })
}

// ---- events and fences ----

pub enum NmbEvent {
    Event(Event),
    Shared(SharedEvent),
}
pub struct NmbFence(Fence);

impl NmbEvent {
    fn as_ref(&self) -> EventRef<'_> {
        match self {
            NmbEvent::Event(e) => EventRef::Event(e),
            NmbEvent::Shared(s) => EventRef::Shared(s),
        }
    }
}

fn shared<'a>(event: *const NmbEvent) -> Result<&'a SharedEvent, String> {
    match get(event)? {
        NmbEvent::Shared(s) => Ok(s),
        NmbEvent::Event(_) => Err("not a shared event".into()),
    }
}

/// One listener thread for every notification of this process (Metal's listener only names the queue a block runs on; the frontend
/// forwards to it).
fn listener() -> Result<&'static SharedEventListener, String> {
    static LISTENER: std::sync::OnceLock<Result<SharedEventListener, String>> = std::sync::OnceLock::new();
    LISTENER.get_or_init(|| SharedEventListener::new().map_err(e("listener"))).as_ref().map_err(|why| why.clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_event_new(device: *mut NmbDevice, is_shared: bool) -> *mut NmbEvent {
    guard(std::ptr::null_mut(), || {
        let device = &get(device)?.0;
        Ok(boxed(if is_shared {
            NmbEvent::Shared(device.new_shared_event().map_err(e("shared event"))?)
        } else {
            NmbEvent::Event(device.new_event().map_err(e("event"))?)
        }))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_event_release(event: *mut NmbEvent) {
    release(event)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_shared_event_value(event: *const NmbEvent) -> u64 {
    guard(0, || Ok(shared(event)?.signaled_value()))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_shared_event_set_value(event: *mut NmbEvent, value: u64) -> bool {
    guard(false, || {
        shared(event)?.set_signaled_value(value);
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_shared_event_wait(event: *mut NmbEvent, value: u64, timeout_ms: u64) -> bool {
    guard(false, || Ok(shared(event)?.wait_until_signaled_value(value, Duration::from_millis(timeout_ms))))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_shared_event_notify(event: *mut NmbEvent, value: u64, handler: Option<extern "C" fn(*mut c_void, u64)>,
                                          context: *mut c_void) -> bool {
    guard(false, || {
        let handler = handler.ok_or("null handler")?;
        let context = Context(context);
        shared(event)?.notify_listener(listener()?, value, Box::new(move |_, reached| {
            let context = context;
            handler(context.0, reached)
        }));
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_signal_event(command_buffer: *mut NmbCommandBuffer, event: *mut NmbEvent, value: u64) -> bool {
    guard(false, || {
        get(command_buffer)?.0.encode_signal_event(get(event)?.as_ref(), value).map_err(e("signal event"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_command_buffer_wait_event(command_buffer: *mut NmbCommandBuffer, event: *mut NmbEvent, value: u64) -> bool {
    guard(false, || {
        get(command_buffer)?.0.encode_wait_for_event(get(event)?.as_ref(), value).map_err(e("wait for event"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_fence_new(device: *mut NmbDevice) -> *mut NmbFence {
    guard(std::ptr::null_mut(), || Ok(boxed(NmbFence(get(device)?.0.new_fence()))))
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_fence_release(fence: *mut NmbFence) {
    release(fence)
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_encoder_update_fence(encoder: *mut NmbEncoder, fence: *mut NmbFence, stages: u64) -> bool {
    guard(false, || {
        let fence = &get(fence)?.0;
        match get_mut(encoder)? {
            NmbEncoder::Compute(c) => c.update_fence(fence),
            NmbEncoder::Blit(b) => b.update_fence(fence),
            NmbEncoder::Render(r) => r.update_fence(fence, raw::<RenderStages>(stages, "render stages")?),
        }
        .map_err(e("update fence"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_encoder_wait_fence(encoder: *mut NmbEncoder, fence: *mut NmbFence, stages: u64) -> bool {
    guard(false, || {
        let fence = &get(fence)?.0;
        match get_mut(encoder)? {
            NmbEncoder::Compute(c) => c.wait_for_fence(fence),
            NmbEncoder::Blit(b) => b.wait_for_fence(fence),
            NmbEncoder::Render(r) => r.wait_for_fence(fence, raw::<RenderStages>(stages, "render stages")?),
        }
        .map_err(e("wait for fence"))?;
        Ok(true)
    })
}

// ---- encoders ----

pub enum NmbEncoder {
    Compute(ComputeCommandEncoder),
    Render(RenderCommandEncoder),
    Blit(BlitCommandEncoder),
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_compute_encoder_new(command_buffer: *mut NmbCommandBuffer) -> *mut NmbEncoder {
    guard(std::ptr::null_mut(), || {
        Ok(boxed(NmbEncoder::Compute(get(command_buffer)?.0.compute_command_encoder().map_err(e("compute encoder"))?)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_blit_encoder_new(command_buffer: *mut NmbCommandBuffer) -> *mut NmbEncoder {
    guard(std::ptr::null_mut(), || Ok(boxed(NmbEncoder::Blit(get(command_buffer)?.0.blit_command_encoder().map_err(e("blit encoder"))?))))
}

#[repr(C)]
pub struct NmbPassAttachment {
    pub texture: *mut NmbTexture,
    pub level: u64,
    pub slice: u64,
    pub depth_plane: u64,
    pub load: u64,
    pub store: u64,
    pub clear: [f64; 4],
}

#[repr(C)]
pub struct NmbRenderPassDescriptor {
    pub colors: [NmbPassAttachment; 8],
    pub depth: NmbPassAttachment,
    pub stencil: NmbPassAttachment,
    pub width: u64,
    pub height: u64,
    pub array_length: u64,
    pub samples: u64,
}

fn texture_of(a: &NmbPassAttachment) -> Result<Option<Texture>, String> {
    if a.texture.is_null() { Ok(None) } else { Ok(Some(get(a.texture)?.0.clone())) }
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_encoder_new(command_buffer: *mut NmbCommandBuffer, descriptor: *const NmbRenderPassDescriptor)
                                         -> *mut NmbEncoder {
    guard(std::ptr::null_mut(), || {
        let d = get(descriptor)?;
        let mut pass = RenderPassDescriptor::new();
        for (slot, a) in pass.color_attachments.iter_mut().zip(d.colors.iter()) {
            slot.texture = texture_of(a)?;
            slot.level = a.level;
            slot.slice = a.slice;
            slot.depth_plane = a.depth_plane;
            slot.load_action = LoadAction::from_raw(a.load).map_err(e("load action"))?;
            slot.store_action = StoreAction::from_raw(a.store).map_err(e("store action"))?;
            slot.clear_color = ClearColor::new(a.clear[0], a.clear[1], a.clear[2], a.clear[3]);
        }
        let depth = &mut pass.depth_attachment;
        depth.texture = texture_of(&d.depth)?;
        depth.level = d.depth.level;
        depth.slice = d.depth.slice;
        depth.depth_plane = d.depth.depth_plane;
        depth.load_action = LoadAction::from_raw(d.depth.load).map_err(e("load action"))?;
        depth.store_action = StoreAction::from_raw(d.depth.store).map_err(e("store action"))?;
        depth.clear_depth = d.depth.clear[0];
        let stencil = &mut pass.stencil_attachment;
        stencil.texture = texture_of(&d.stencil)?;
        stencil.level = d.stencil.level;
        stencil.slice = d.stencil.slice;
        stencil.depth_plane = d.stencil.depth_plane;
        stencil.load_action = LoadAction::from_raw(d.stencil.load).map_err(e("load action"))?;
        stencil.store_action = StoreAction::from_raw(d.stencil.store).map_err(e("store action"))?;
        stencil.clear_stencil = d.stencil.clear[0] as u32;
        pass.render_target_width = d.width;
        pass.render_target_height = d.height;
        pass.render_target_array_length = d.array_length;
        pass.default_raster_sample_count = d.samples;
        let encoder = get(command_buffer)?.0.render_command_encoder(&pass).map_err(e("render encoder"))?;
        Ok(boxed(NmbEncoder::Render(encoder)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_encoder_end(encoder: *mut NmbEncoder) -> bool {
    guard(false, || {
        if encoder.is_null() {
            return Err("null encoder".into());
        }
        // SAFETY: from `boxed`; ending releases it.
        let encoder = unsafe { Box::from_raw(encoder) };
        match *encoder {
            NmbEncoder::Compute(c) => c.end_encoding(),
            NmbEncoder::Render(r) => r.end_encoding(),
            NmbEncoder::Blit(b) => b.end_encoding(),
        }
        .map_err(e("end encoding"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_encoder_discard(encoder: *mut NmbEncoder) {
    release(encoder)
}

fn compute<'a>(encoder: *mut NmbEncoder) -> Result<&'a mut ComputeCommandEncoder, String> {
    match get_mut(encoder)? {
        NmbEncoder::Compute(c) => Ok(c),
        _ => Err("not a compute encoder".into()),
    }
}

fn render<'a>(encoder: *mut NmbEncoder) -> Result<&'a mut RenderCommandEncoder, String> {
    match get_mut(encoder)? {
        NmbEncoder::Render(r) => Ok(r),
        _ => Err("not a render encoder".into()),
    }
}

fn blit<'a>(encoder: *mut NmbEncoder) -> Result<&'a mut BlitCommandEncoder, String> {
    match get_mut(encoder)? {
        NmbEncoder::Blit(b) => Ok(b),
        _ => Err("not a blit encoder".into()),
    }
}

fn optional<'a, T>(handle: *const T) -> Result<Option<&'a T>, String> {
    if handle.is_null() { Ok(None) } else { get(handle).map(Some) }
}

const COMPUTE: u32 = 0;
const VERTEX: u32 = 1;
const FRAGMENT: u32 = 2;

fn bad_stage(stage: u32) -> String {
    format!("stage {stage} does not fit this encoder")
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_encoder_set_buffer(encoder: *mut NmbEncoder, stage: u32, buffer: *mut NmbBuffer, offset: u64, index: u32) -> bool {
    guard(false, || {
        let buffer = optional(buffer)?.map(|b| &b.0);
        match (get_mut(encoder)?, stage) {
            (NmbEncoder::Compute(c), COMPUTE) => c.set_buffer(buffer, offset, index),
            (NmbEncoder::Render(r), VERTEX) => r.set_vertex_buffer(buffer, offset, index),
            (NmbEncoder::Render(r), FRAGMENT) => r.set_fragment_buffer(buffer, offset, index),
            _ => return Err(bad_stage(stage)),
        }
        .map_err(e("set buffer"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_encoder_set_buffer_offset(encoder: *mut NmbEncoder, stage: u32, offset: u64, index: u32) -> bool {
    guard(false, || {
        match (get_mut(encoder)?, stage) {
            (NmbEncoder::Compute(c), COMPUTE) => c.set_buffer_offset(offset, index),
            (NmbEncoder::Render(r), VERTEX) => r.set_vertex_buffer_offset(offset, index),
            (NmbEncoder::Render(r), FRAGMENT) => r.set_fragment_buffer_offset(offset, index),
            _ => return Err(bad_stage(stage)),
        }
        .map_err(e("set buffer offset"))?;
        Ok(true)
    })
}

/// # Safety
/// `bytes` holds `length` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nmb_encoder_set_bytes(encoder: *mut NmbEncoder, stage: u32, bytes: *const c_void, length: usize, index: u32)
                                               -> bool {
    guard(false, || {
        if bytes.is_null() {
            return Err("null bytes".into());
        }
        // SAFETY: the caller's contract.
        let bytes = unsafe { std::slice::from_raw_parts(bytes as *const u8, length) };
        match (get_mut(encoder)?, stage) {
            (NmbEncoder::Compute(c), COMPUTE) => c.set_bytes(bytes, index),
            (NmbEncoder::Render(r), VERTEX) => r.set_vertex_bytes(bytes, index),
            (NmbEncoder::Render(r), FRAGMENT) => r.set_fragment_bytes(bytes, index),
            _ => return Err(bad_stage(stage)),
        }
        .map_err(e("set bytes"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_encoder_set_texture(encoder: *mut NmbEncoder, stage: u32, texture: *mut NmbTexture, index: u32) -> bool {
    guard(false, || {
        let texture = optional(texture)?.map(|t| &t.0);
        match (get_mut(encoder)?, stage) {
            (NmbEncoder::Compute(c), COMPUTE) => c.set_texture(texture, index),
            (NmbEncoder::Render(r), VERTEX) => r.set_vertex_texture(texture, index),
            (NmbEncoder::Render(r), FRAGMENT) => r.set_fragment_texture(texture, index),
            _ => return Err(bad_stage(stage)),
        }
        .map_err(e("set texture"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_encoder_set_sampler(encoder: *mut NmbEncoder, stage: u32, sampler: *mut NmbSampler, index: u32) -> bool {
    guard(false, || {
        let sampler = optional(sampler)?.map(|s| &s.0);
        match (get_mut(encoder)?, stage) {
            (NmbEncoder::Compute(c), COMPUTE) => c.set_sampler_state(sampler, index),
            (NmbEncoder::Render(r), VERTEX) => r.set_vertex_sampler_state(sampler, index),
            (NmbEncoder::Render(r), FRAGMENT) => r.set_fragment_sampler_state(sampler, index),
            _ => return Err(bad_stage(stage)),
        }
        .map_err(e("set sampler"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_compute_set_pipeline(encoder: *mut NmbEncoder, pipeline: *mut NmbComputePipeline) -> bool {
    guard(false, || {
        compute(encoder)?.set_compute_pipeline_state(&get(pipeline)?.0).map_err(e("compute pipeline"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_compute_set_threadgroup_memory(encoder: *mut NmbEncoder, length: u32, index: u32) -> bool {
    guard(false, || {
        compute(encoder)?.set_threadgroup_memory_length(length, index).map_err(e("threadgroup memory"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_compute_dispatch(encoder: *mut NmbEncoder, size: *const u64, per_group: *const u64, threads: bool) -> bool {
    guard(false, || {
        let (s, g) = (triple(size)?, triple(per_group)?);
        let (s, g) = (Size::new(s[0], s[1], s[2]), Size::new(g[0], g[1], g[2]));
        let c = compute(encoder)?;
        if threads { c.dispatch_threads(s, g) } else { c.dispatch_threadgroups(s, g) }.map_err(e("dispatch"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_set_pipeline(encoder: *mut NmbEncoder, pipeline: *mut NmbRenderPipeline) -> bool {
    guard(false, || {
        render(encoder)?.set_render_pipeline_state(&get(pipeline)?.0).map_err(e("render pipeline"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_set_depth_stencil(encoder: *mut NmbEncoder, state: *mut NmbDepthStencil) -> bool {
    guard(false, || {
        render(encoder)?.set_depth_stencil_state(optional(state)?.map(|s| &s.0)).map_err(e("depth/stencil state"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_set_viewport(encoder: *mut NmbEncoder, viewport: *const f64) -> bool {
    guard(false, || {
        if viewport.is_null() {
            return Err("null viewport".into());
        }
        // SAFETY: the header's six values.
        let v = unsafe { std::slice::from_raw_parts(viewport, 6) };
        render(encoder)?.set_viewport(Viewport::new(v[0], v[1], v[2], v[3], v[4], v[5])).map_err(e("viewport"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_set_scissor(encoder: *mut NmbEncoder, rect: *const u64) -> bool {
    guard(false, || {
        if rect.is_null() {
            return Err("null scissor".into());
        }
        // SAFETY: the header's four values.
        let r = unsafe { std::slice::from_raw_parts(rect, 4) };
        render(encoder)?.set_scissor_rect(ScissorRect::new(r[0], r[1], r[2], r[3])).map_err(e("scissor"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_set_raster(encoder: *mut NmbEncoder, cull_mode: u64, winding: u64, fill_mode: u64) -> bool {
    guard(false, || {
        let r = render(encoder)?;
        r.set_cull_mode(raw::<CullMode>(cull_mode, "cull mode")?);
        r.set_front_facing_winding(raw::<Winding>(winding, "winding")?);
        r.set_triangle_fill_mode(raw::<TriangleFillMode>(fill_mode, "fill mode")?).map_err(e("fill mode"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_set_stencil_reference(encoder: *mut NmbEncoder, front: u32, back: u32) -> bool {
    guard(false, || {
        render(encoder)?.set_stencil_reference_values(front, back);
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_set_blend_color(encoder: *mut NmbEncoder, color: *const f32) -> bool {
    guard(false, || {
        if color.is_null() {
            return Err("null color".into());
        }
        // SAFETY: the header's four values.
        let c = unsafe { std::slice::from_raw_parts(color, 4) };
        render(encoder)?.set_blend_color(c[0], c[1], c[2], c[3]);
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_render_draw(encoder: *mut NmbEncoder, primitive: u64, start: u32, count: u32, instances: u32, base_instance: u32)
                                  -> bool {
    guard(false, || {
        render(encoder)?.draw_primitives(raw::<PrimitiveType>(primitive, "primitive")?, start, count, instances, base_instance)
            .map_err(e("draw"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn nmb_render_draw_indexed(encoder: *mut NmbEncoder, primitive: u64, count: u32, index_type: u64, indices: *mut NmbBuffer,
                                          offset: u64, instances: u32, base_vertex: i32, base_instance: u32) -> bool {
    guard(false, || {
        render(encoder)?.draw_indexed_primitives(raw::<PrimitiveType>(primitive, "primitive")?, count,
                                                 raw::<IndexType>(index_type, "index type")?, &get(indices)?.0, offset, instances,
                                                 base_vertex, base_instance).map_err(e("indexed draw"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_blit_copy_buffer(encoder: *mut NmbEncoder, source: *mut NmbBuffer, source_offset: u64, destination: *mut NmbBuffer,
                                       destination_offset: u64, size: u64) -> bool {
    guard(false, || {
        blit(encoder)?.copy_from_buffer_to_buffer(&get(source)?.0, source_offset, &get(destination)?.0, destination_offset, size)
            .map_err(e("buffer copy"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_blit_fill_buffer(encoder: *mut NmbEncoder, buffer: *mut NmbBuffer, start: u64, end: u64, value: u8) -> bool {
    guard(false, || {
        blit(encoder)?.fill_buffer(&get(buffer)?.0, start..end, value).map_err(e("fill"))?;
        Ok(true)
    })
}

fn origin(v: *const u64) -> Result<Origin, String> {
    let v = triple(v)?;
    Ok(Origin::new(v[0], v[1], v[2]))
}

fn size(v: *const u64) -> Result<Size, String> {
    let v = triple(v)?;
    Ok(Size::new(v[0], v[1], v[2]))
}

#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn nmb_blit_copy_texture(encoder: *mut NmbEncoder, source: *mut NmbTexture, source_slice: u64, source_level: u64,
                                        source_origin: *const u64, extent: *const u64, destination: *mut NmbTexture,
                                        destination_slice: u64, destination_level: u64, destination_origin: *const u64) -> bool {
    guard(false, || {
        blit(encoder)?.copy_from_texture_to_texture(&get(source)?.0, source_slice, source_level, origin(source_origin)?, size(extent)?,
                                                    &get(destination)?.0, destination_slice, destination_level,
                                                    origin(destination_origin)?).map_err(e("texture copy"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn nmb_blit_buffer_to_texture(encoder: *mut NmbEncoder, source: *mut NmbBuffer, offset: u64, bytes_per_row: u64,
                                             bytes_per_image: u64, extent: *const u64, destination: *mut NmbTexture, slice: u64,
                                             level: u64, destination_origin: *const u64) -> bool {
    guard(false, || {
        blit(encoder)?.copy_from_buffer_to_texture(&get(source)?.0, offset, bytes_per_row, bytes_per_image, size(extent)?,
                                                   &get(destination)?.0, slice, level, origin(destination_origin)?, BlitOption::None)
            .map_err(e("buffer to texture"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn nmb_blit_texture_to_buffer(encoder: *mut NmbEncoder, source: *mut NmbTexture, slice: u64, level: u64,
                                             source_origin: *const u64, extent: *const u64, destination: *mut NmbBuffer, offset: u64,
                                             bytes_per_row: u64, bytes_per_image: u64) -> bool {
    guard(false, || {
        blit(encoder)?.copy_from_texture_to_buffer(&get(source)?.0, slice, level, origin(source_origin)?, size(extent)?,
                                                   &get(destination)?.0, offset, bytes_per_row, bytes_per_image, BlitOption::None)
            .map_err(e("texture to buffer"))?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn nmb_blit_generate_mipmaps(encoder: *mut NmbEncoder, texture: *mut NmbTexture) -> bool {
    guard(false, || {
        blit(encoder)?.generate_mipmaps(&get(texture)?.0).map_err(e("mipmaps"))?;
        Ok(true)
    })
}
