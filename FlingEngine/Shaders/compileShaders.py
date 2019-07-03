import os;
import sys;
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
			outFileaName = Path(filename).stem;

			# Find the name that we should output to
			print("Out file name: " + outFileaName);

			# Compile the shader 
			call([ 
				os.environ['VK_SDK_PATH'] + "/Bin/glslangValidator.exe",
				"-V",
				filename
				#"-o", outFileaName + ".spv"
			]);

#TODO: Setup sys args for building/cleaning more specifically

buildShaders();