import subprocess
import ninja_syntax
import struct
import winreg
import os

SHADERS_DIR : str = "../Source/Shaders/"
INTERMEDIATE_DIR : str = "../CompiledShaders/"

class WinSDKHelper:
	def __init__(self):
		roots_key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots")
		num_subkeys = winreg.QueryInfoKey(roots_key)[0]
		subkeys = list(map(lambda i: winreg.EnumKey(roots_key, i), range(num_subkeys)))
		self.latest_sdk = max(subkeys, key = lambda version_str: version_str.split(".")[2])
		self.bin_path = f"{winreg.QueryValueEx(roots_key, 'KitsRoot10')[0]}\\bin\\{self.latest_sdk}\\x64"
		self.dxc_path = f"{self.bin_path}\\dxc.exe"
		pass

class ShaderCompiler:
	def __init__(self, configs):
		self.configs = configs
		self.ninja_filename = f"{INTERMEDIATE_DIR}/shaders.ninja"

		pass
	
	def generate_ninja(self):
		winsdk_helper = WinSDKHelper()
		ninja = ninja_syntax.Writer(open(self.ninja_filename, "w+"))
		ninja.rule("dxc", f"\"{winsdk_helper.dxc_path}\" $in -Fh $out -Od -Zi -Qembed_debug -all_resources_bound -H -T $target -Vn $header $defines")


		header_files = []

		for shader_config in self.configs:
			filename = f"{SHADERS_DIR}/{shader_config['file']}"
			file_path = os.path.normpath(filename)
			name, _ = os.path.splitext(os.path.basename(file_path))
			header, _ = os.path.splitext(name)

			defines = dict()
			
			dxil_header = f"{name}_Compiled.h" 
			header_files.append(dxil_header)
			target_map = {
				"CS" : "cs_6_0",
				"VS" : "vs_6_0",
				"PS" : "ps_6_0"
			}

			ninja.build(dxil_header, "dxc", filename, variables = {"target" : target_map[shader_config["type"]], "header" : f"{header}_ShaderData"} | defines)

		with open(f"{INTERMEDIATE_DIR}/CompiledShaders.h", "w+") as compiled_shaders_h:
			for header_file in header_files:
				compiled_shaders_h.write(f"#include \"{os.path.basename(header_file)}\"\n")
	
	def compile_shaders(self):
		print("Running ninja...")
		subprocess.run(f"ninja.exe -f {self.ninja_filename} -C {INTERMEDIATE_DIR}")

def main():
	shaders = [
		{"file" : "DebugPixel.hlsl", "type" : "PS"},
		{"file" : "DebugVertex.hlsl", "type" : "VS"},
		{"file" : "FullScreenTriangle.hlsl", "type" : "VS"},
		{"file" : "HorizontalBlur.hlsl", "type" : "CS"},
		{"file" : "LightingToScreen.hlsl", "type" : "PS"},
		{"file" : "LightPixel.hlsl", "type" : "PS"},
		{"file" : "LightVertex.hlsl", "type" : "VS"},
		{"file" : "PixelShader.hlsl", "type" : "PS"},
		{"file" : "ResolveLighting.hlsl", "type" : "CS"},
		{"file" : "VertexShader.hlsl", "type" : "VS"},
		{"file" : "VerticalBlur.hlsl", "type" : "CS"},
		{"file" : "UAVClear.hlsl", "type" : "CS"},
		{"file" : "Down.hlsl", "type" : "CS"},
		{"file" : "CopyDepth.hlsl", "type" : "PS"}
	]
	shader_compiler = ShaderCompiler(shaders)
	shader_compiler.generate_ninja()
	shader_compiler.compile_shaders()
	pass

if __name__ == "__main__":
	main()
