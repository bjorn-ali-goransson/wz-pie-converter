#include <stdio.h>
#include <fstream>
#include <kaitai/kaitaistream.h>
#include "blender_blend.h"

class DataBlock {
	public:
	DataBlock(blender_blend_t &data, std::string code){
		for(auto &block : *data.blocks()){
			// ->code() is fixed-length and padded with null characters making string comparison impossible
			auto blockCode = std::string(block->code().c_str());
			
			if(blockCode != code){
				continue;
			}

			auto &type = *data.sdna_structs()->at(block->sdna_index());

			printf("%s (%s)\n", block->code().c_str(), type.type().c_str());
			
			for(auto &_field : *type.fields()){
				printf("%s (%s)\n", _field->name().c_str(), _field->type().c_str());
			}
		}
	}
};

int main(int argc, char **argv) {
	printf("Starting\n");
	std::ifstream is("/home/bjorn/Desktop/blender-convert/cube.blend", std::ifstream::binary);
	kaitai::kstream ks(&is);
	blender_blend_t data(&ks);

	printf("File blender version: %s\n", data.hdr()->version().c_str()); // => get hdr

	std::unique_ptr<DataBlock> mesh = std::unique_ptr<DataBlock>(new DataBlock(data, "ME"));

	// this is for listing data types
	// for(auto &sdna_struct : *data.sdna_structs()){
	// 	printf("%s\n", sdna_struct->type().c_str());
	// 	for(auto &field : *sdna_struct->fields()){
	// 		printf("  %s (%s)\n", field->name().c_str(), field->type().c_str());
	// 	}
	// 	printf("\n");
	// }

	return 0;
}
