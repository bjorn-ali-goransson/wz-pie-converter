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

	auto &structs = *data.sdna_structs();

	// for(auto &_struct : *data.sdna_structs()){
	// 	if(_struct->type() != "Mesh"){
	// 		continue;
	// 	}

	// 	for(auto &_field : *_struct->fields()){
	// 		printf("%s (%s)\n", _field->name().c_str(), _field->type().c_str());
	// 	}
	// }

	for(auto &block : *data.blocks()){
		if(block->count() == 0){
			continue;
		}

		printf("%s[%i = %s] (structs: %i)\n", block->code().c_str(), block->sdna_index(), structs.at(block->sdna_index())->type().c_str(), block->count());

		if(block->code() != "DNA1"){
			continue;
		}

		printf("Block (%s)\n", block->code().c_str());

		auto &body = *block->body();

		// auto &body = *block->count();

		// printf("Types (%i): \n", body.count());

		// for(auto &type : *block->body()->types()){
		// 	printf("  %s\n", type.c_str());
		// }
	}

	return 0;
}


