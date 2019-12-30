import os;
from subprocess import call;
from pathlib import Path

def cleanShaders():
	# For each file in the current directory
	for filename in os.listdir('.'):
		if filename.endswith(".spv"):
			print (filename);

def buildShaders():

	# For each file in the current directory
	for filename in os.listdir('.'):
		if filename.endswith(".frag") or filename.endswith(".vert"):
			outFileName = Path(filename).stem;

			if filename.endswith(".frag"):
				outFileName += "_frag";
			elif filename.endswith(".vert"):
				outFileName += "_vert";

			outFileName += ".spv"
			# Find the name that we should output to
			print("Out file name: " + outFileName);

			# Compile the shader
			call([
				os.environ['VK_BIN_PATH'] + "/glslangValidator",
				"-V",
				filename,
				"-o",
				outFileName
			]);

#TODO: Setup sys args for building/cleaning more specifically

buildShaders();
