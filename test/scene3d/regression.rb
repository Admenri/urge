# Run as an RGSS script in Game.exe. Results go to scene3d-results.txt.
$stdout.sync = true
RESULTS = []
KEEP = []

def check(name)
  yield
  RESULTS << "#{name}: OK"
rescue => error
  RESULTS << "#{name}: FAIL #{error.class}: #{error.message}"
ensure
  KEEP.each { |world| world.dispose }
  KEEP.clear
end

def assert(value, message)
  raise message unless value
end

def rejects
  raised = false
  begin
    yield
  rescue
    raised = true
  end
  assert(raised, "invalid configuration was accepted")
end

FULL_VS = <<'WGSL'
@vertex fn vs_main(@builtin(vertex_index) i: u32) -> @builtin(position) vec4<f32> {
  return urge_fullscreen_position(i);
}
WGSL

SAMPLE_FS = <<'WGSL'
@group(2) @binding(0) var source: texture_2d<f32>;
@fragment fn fs_main(@builtin(position) p: vec4<f32>) -> @location(0) vec4<f32> {
  return textureLoad(source, vec2<i32>(0, 0), 0);
}
WGSL

def fullscreen(graph, name, fragment, colors = [])
  material = Material3D.new
  shader = material.add_pass(name, FULL_VS, fragment)
  shader.cull = 1
  pass = graph.add_pass(name)
  pass.kind = 1
  pass.material = shader
  pass.colors = colors
  pass
end

def world_for(graph, camera = nil, scene = nil)
  camera ||= Camera3D.new
  scene ||= Scene3D.new
  world = World3D.new(0, 0, 64, 64)
  world.camera = camera
  world.scene = scene
  world.graph = graph
  KEEP << world
  world
end

def pixel
  2.times { Graphics.update }
  bitmap = Graphics.snap_to_bitmap
  color = bitmap.get_pixel(32, 32)
  rgb = [color.red, color.green, color.blue]
  bitmap.dispose
  rgb
end

def near(actual, expected, tolerance = 8)
  assert((actual - expected).abs <= tolerance, "expected #{expected}, got #{actual}")
end

check("configuration validation") do
  rejects { RenderTarget3D.new(16, 16, "rgba8unorm", "depth32float", 2) }
  rejects { RenderTarget3D.new(16, 16, "rgba32float", "depth32float", 4) }
  rejects { RenderTarget3D.new(16, 16, "rg11b10ufloat", "depth32float", 1) }
  graph = RenderGraph3D.new
  rejects { graph.add_texture("screen", "rgba8unorm", 1) }
  rejects { graph.add_storage_texture("depth", "depth32float") }
  rejects { graph.add_storage_texture("rg", "rg16float") }
  rejects { graph.add_storage_texture("r", "r8unorm") }
  rejects { graph.add_storage_texture("snorm", "rgba8snorm") }
  graph.add_texture("valid", "rgba8unorm", 1)
  rejects { graph.set_texture_scale("valid", Float::NAN) }
  camera = Camera3D.new
  camera.far = 1.0
  camera.near = 2.0
  assert(camera.far > camera.near, "far <= near")
  camera.far = 10.0
  near(camera.shadow_splits(4, 0.5, 100.0).last, 10.0, 0.001)
end

check("detached pass and scene ownership") do
  graph = RenderGraph3D.new
  pass = graph.add_pass("retained")
  graph.remove_pass("retained")
  assert(pass.texture("any").nil?, "removed pass still has an owner")
  pass = graph.add_pass("cleared")
  graph.clear
  assert(pass.texture("any").nil?, "cleared pass still has an owner")
  pass = graph.add_pass("collected")
  graph = nil
  GC.start
  assert(pass.texture("any").nil?, "collected graph still has an owner")
  parent = Node3D.new
  child = Node3D.new
  parent.add_child(child)
  child.add_child(parent)
  assert(parent.parent.nil? && !child.parent.nil? && parent.child_count == 1,
         "ancestor cycle accepted")
  parent.add_child(parent)
  assert(parent.parent.nil?, "self cycle accepted")
end

check("20 pass view and globals isolation") do
  graph = RenderGraph3D.new
  fragment = <<'WGSL'
@fragment fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(u_view.camera_position.x / 20.0, u_globals.viewport.x / 64.0, 0.0, 1.0);
}
WGSL
  20.times do |i|
    camera = Camera3D.new
    camera.position = Vector3.new(i.to_f, 0.0, 0.0)
    pass = fullscreen(graph, "view#{i}", fragment)
    pass.camera = camera
  end
  world_for(graph)
  rgb = pixel
  near(rgb[0], 242)
  near(rgb[1], 255)
end

check("forward producer inferred from binding") do
  graph = RenderGraph3D.new
  graph.add_texture("input", "rgba8unorm", 1)
  consumer = fullscreen(graph, "consumer", SAMPLE_FS)
  consumer.bind_texture("source", "input")
  producer = graph.add_pass("producer")
  producer.colors = ["input"]
  producer.clear_color = Color.new(0, 255, 0, 255)
  world_for(graph)
  near(pixel[1], 255)
end

check("MRT identity, scale and recreation") do
  graph = RenderGraph3D.new
  ["a", "b"].each do |name|
    graph.add_texture(name, "rgba8unorm", 1)
    graph.set_texture_scale(name, 0.5)
  end
  fragment = <<'WGSL'
struct Output { @location(0) a: vec4<f32>, @location(1) b: vec4<f32> };
@fragment fn fs_main() -> Output {
  var out: Output;
  out.a = vec4<f32>(u_globals.viewport.x / 64.0, 0.0, 0.0, 1.0);
  out.b = vec4<f32>(0.0, 0.0, 1.0, 1.0);
  return out;
}
WGSL
  fullscreen(graph, "mrt", fragment, ["a", "b"])
  rewrite = graph.add_pass("rewrite_b")
  rewrite.colors = ["b"]
  rewrite.clear_color = Color.new(0, 255, 0, 255)
  consumer = fullscreen(graph, "consumer", SAMPLE_FS)
  consumer.bind_texture("source", "b")
  world_for(graph)
  rgb = pixel
  near(rgb[1], 255)
  near(rgb[2], 0)
  consumer.bind_texture("source", "a")
  near(pixel[0], 128)
  graph.add_texture("a", "rgba16float", 1)
  graph.set_texture_scale("a", 0.25)
  graph.set_texture_scale("b", 0.25)
  near(pixel[0], 64)
end

check("MSAA color resolve with depth") do
  graph = RenderGraph3D.new
  graph.add_texture("msaa", "rgba8unorm", 4)
  fullscreen(graph, "msaa_draw", <<'WGSL', ["msaa"])
@fragment fn fs_main() -> @location(0) vec4<f32> { return vec4<f32>(1.0, 0.0, 0.0, 1.0); }
WGSL
  consumer = fullscreen(graph, "consumer", SAMPLE_FS)
  consumer.bind_texture("source", "msaa")
  world_for(graph)
  near(pixel[0], 255)
end

check("camera override rechecks layers and frustum") do
  graph = RenderGraph3D.new
  initial = Camera3D.new
  initial.culling_mask = 1
  initial.position = Vector3.new(20.0, 0.0, 0.0)
  override = Camera3D.new
  override.culling_mask = 2
  material = Material3D.new
  material.add_pass("opaque", <<'WGSL', <<'WGSL')
@vertex fn vs_main(@location(0) p: vec3<f32>) -> @builtin(position) vec4<f32> {
  return u_view.view_proj * urge_world(vec4<f32>(p, 1.0));
}
WGSL
@fragment fn fs_main() -> @location(0) vec4<f32> { return vec4<f32>(1.0, 0.0, 0.0, 1.0); }
WGSL
  mesh = Mesh3D.new
  mesh.set_vertices([-2, -2, -3, 0, 0, 1, 0, 0, 2, -2, -3, 0, 0, 1, 1, 0,
                     0, 2, -3, 0, 0, 1, 0.5, 1])
  mesh.set_indices([0, 1, 2])
  renderer = MeshRenderer3D.new
  renderer.mesh = mesh
  renderer.layers = 2
  renderer.cast_shadow = false
  renderer.set_material(0, material)
  scene = Scene3D.new
  scene.add_child(renderer)
  pass = graph.add_pass("override")
  pass.tags = ["opaque"]
  pass.camera = override
  world_for(graph, initial, scene)
  near(pixel[0], 255)
  fullscreen(graph, "default_camera", <<'WGSL')
@fragment fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(u_view.camera_position.x / 20.0, 0.0, 0.0, 1.0);
}
WGSL
  near(pixel[0], 255)
end

check("shadow disable and invisible light reset") do
  graph = RenderGraph3D.new
  shadow = graph.add_pass("shadow")
  shadow.depth_only = true
  shadow.cascades = 4
  fullscreen(graph, "count", <<'WGSL')
@fragment fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(f32(urge_cascade_count()) / 4.0, 0.0, 0.0, 1.0);
}
WGSL
  scene = Scene3D.new
  light = Light3D.new
  light.cast_shadow = true
  scene.add_child(light)
  world_for(graph, nil, scene)
  near(pixel[0], 255)
  shadow.enabled = false
  near(pixel[0], 0)
  shadow.enabled = true
  light.visible = false
  near(pixel[0], 0)
end

check("compute graph bindings and independent storage buffers") do
  graph = RenderGraph3D.new
  noop = graph.add_pass("compute_without_bindings")
  noop.kind = 2
  noop.compute = ComputePass3D.new("@compute @workgroup_size(1) fn cs_main() {}", "cs_main")
  graph.add_storage_texture("computed", "rgba8unorm")
  compute = ComputePass3D.new(<<'WGSL', "cs_main")
@group(0) @binding(0) var<storage, read_write> accum: array<f32>;
@group(0) @binding(1) var<storage, read> other: array<f32>;
@group(0) @binding(2) var image: texture_storage_2d<rgba8unorm, write>;
@compute @workgroup_size(1) fn cs_main() {
  accum[0] = accum[0] + 1.0;
  textureStore(image, vec2<i32>(0, 0), vec4<f32>(accum[0] / 20.0, other[0], 0.0, 1.0));
}
WGSL
  compute.set_buffer("accum", [0.0])
  compute.set_buffer("other", [1.0])
  pass = graph.add_pass("compute")
  pass.kind = 2
  pass.colors = ["computed"]
  pass.compute = compute
  pass.bind_texture("image", "computed")
  consumer = fullscreen(graph, "consumer", SAMPLE_FS)
  consumer.bind_texture("source", "computed")
  world_for(graph)
  first = pixel
  compute.set_buffer("other", [0.5])
  second = pixel
  assert(second[0] > first[0] + 10, "updating other reset accumulation: #{first}, #{second}")
  near(second[1], 128)
end

check("vertex material bindings in shadow and raster") do
  scene = Scene3D.new
  light = Light3D.new
  light.cast_shadow = true
  scene.add_child(light)
  material = Material3D.new
  shader = material.add_pass("opaque", <<'WGSL', <<'WGSL')
@group(2) @binding(0) var<uniform> displacement: vec4<f32>;
@group(2) @binding(1) var height: texture_2d<f32>;
@group(2) @binding(2) var height_sampler: sampler;
@vertex fn vs_main(@location(0) p: vec3<f32>) -> @builtin(position) vec4<f32> {
  let h = textureSampleLevel(height, height_sampler, vec2<f32>(0.5), 0.0).r;
  return u_view.view_proj * urge_world(vec4<f32>(p + displacement.xyz * h, 1.0));
}
WGSL
@fragment fn fs_main() -> @location(0) vec4<f32> { return vec4<f32>(1.0, 0.0, 0.0, 1.0); }
WGSL
  shader.set_uniform("displacement", [0.0, 0.0, 0.0, 0.0])
  mesh = Mesh3D.new
  mesh.set_vertices([-2, -2, -3, 0, 0, 1, 0, 0, 2, -2, -3, 0, 0, 1, 1, 0,
                     0, 2, -3, 0, 0, 1, 0.5, 1])
  mesh.set_indices([0, 1, 2])
  renderer = MeshRenderer3D.new
  renderer.mesh = mesh
  renderer.set_material(0, material)
  scene.add_child(renderer)
  world_for(ForwardPipeline3D.new, nil, scene)
  near(pixel[0], 255)
end

check("invalid graphs report errors and recover") do
  graph = RenderGraph3D.new
  graph.add_texture("a", "rgba8unorm", 1)
  graph.add_texture("b", "rgba8unorm", 1)
  a = graph.add_pass("cycle_a")
  a.colors = ["a"]
  a.reads = ["b"]
  b = graph.add_pass("cycle_b")
  b.colors = ["b"]
  b.reads = ["a"]
  world_for(graph)
  2.times { Graphics.update }
  graph.clear
  missing = graph.add_pass("missing_producer")
  missing.reads = ["missing"]
  2.times { Graphics.update }
  graph.clear
  graph.add_texture("self", "rgba8unorm", 1)
  feedback = fullscreen(graph, "feedback", SAMPLE_FS, ["self"])
  feedback.bind_texture("source", "self")
  2.times { Graphics.update }
  graph.clear
  graph.add_texture("small", "rgba8unorm", 1)
  graph.add_texture("large", "rgba8unorm", 1)
  graph.set_texture_scale("small", 0.5)
  mismatched = graph.add_pass("mismatched_mrt")
  mismatched.colors = ["small", "large"]
  2.times { Graphics.update }
  graph.clear
  graph.add_texture("shadow", "rgba8unorm", 1)
  collision = graph.add_pass("shadow_collision")
  collision.depth_only = true
  collision.colors = ["shadow"]
  2.times { Graphics.update }
  graph.clear
  recovered = graph.add_pass("recovered")
  recovered.clear_color = Color.new(0, 255, 0, 255)
  near(pixel[1], 255)
end

File.write("scene3d-results.txt", RESULTS.join("\n") + "\n")
exit!(0)
