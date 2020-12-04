#include <stdio.h>
#include <fstream>
#include <kaitai/kaitaistream.h>
#include "blender_blend.h"

int main(int argc, char **argv) {
	printf("Starting\n");
	std::ifstream is("/home/bjorn/Desktop/blender-convert/cube.blend", std::ifstream::binary);
	kaitai::kstream ks(&is);
	blender_blend_t data(&ks);

	printf("File blender version: %s\n", data.hdr()->version().c_str()); // => get hdr

	printf("Number of blocks: %i\n", (int)data.blocks()->size());

	for(auto &block : *data.blocks()){
		printf("Types: \n");

		for(auto &type : *block->body()->types()){
			printf("  %s\n", type.c_str());
		}
	}

	return 0;
}


