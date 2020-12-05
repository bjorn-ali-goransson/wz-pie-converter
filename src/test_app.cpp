#include <stdio.h>
#include <fstream>
#include <kaitai/kaitaistream.h>
#include "blender_blend.h"

class DataSource {
	private:
	std::string raw_body;
	public:
	DataSource(std::string raw_body){
		this->raw_body = raw_body;
	}
	std::unique_ptr<kaitai::kstream> getStream(size_t offset){
		auto stream = new kaitai::kstream(raw_body);
		stream->seek(offset);
		return std::unique_ptr<kaitai::kstream>(stream);
	}
};

class DataPart {
	private:
	blender_blend_t *data;
	DataSource *dataSource;
	size_t offset;
	std::unique_ptr<kaitai::kstream> stream;
	blender_blend_t::dna_struct_t *type;

	public:
	DataPart(blender_blend_t &data, DataSource &dataSource, size_t pos, blender_blend_t::dna_struct_t &type){
		this->data = &data;
		this->dataSource = &dataSource;
		this->offset = pos;
		this->stream = dataSource.getStream(pos);
		this->type = &type;
	}

	std::unique_ptr<DataPart> getPart(std::string name){
		for(auto &field : *type->fields()){
			if(field->name() != name){
				continue;
			}

			blender_blend_t::dna_struct_t *fieldType = nullptr;

			for(auto &sdna_struct : *data->sdna_structs()){
				if(sdna_struct->type() != field->type()){
					continue;
				}

				fieldType = &*sdna_struct;
			}

			return std::unique_ptr<DataPart>(new DataPart(*data, *dataSource, offset, *fieldType));
		}

		return nullptr;
	}

	std::string getString(std::string name){
		blender_blend_t::dna_field_t *field = nullptr;

		for(auto &f : *type->fields()){
			if(f->name() == name){
				field = &*f;
				break;
			}

			int bracketPosition = f->name().find('[');

			if(bracketPosition != -1 && f->name().substr(0, bracketPosition) == name){
				field = &*f;
				break;
			}
		}

		if(field == nullptr){
			throw std::runtime_error(std::string("Could not find field ") + name + " on type " + type->type());
		}

		int offset = getOffsetOf(field->name());
		int size = getArrayLength(field->name());

		stream->seek(offset);
		return kaitai::kstream::bytes_to_str(stream->read_bytes(66), std::string("ASCII"));
	}

	/// Gets the memory offset of the field with the complete name, including pointer and array specifiers.
	int getArrayLength(std::string name){
		int bracketPosition = name.find('[');

		if(bracketPosition == -1){
			return 1;
		}

		int bracketEnd = name.find(']');

		if(bracketEnd == -1){
			throw std::runtime_error(std::string("Array syntax was not name[length] - no end bracket"));
		}
		if(bracketEnd != name.length() - 1){
			throw std::runtime_error(std::string("Array syntax was not name[length] - something came after last bracket"));
		}

		auto arrayLengthString = name.substr(bracketPosition + 1, bracketEnd - bracketPosition);

		return std::stoi(arrayLengthString);
	}

	/// Gets the memory offset of the field with the complete name, including pointer and array specifiers.
	int getOffsetOf(std::string name){
		int internalOffset = 0;

		for(auto &field : *type->fields()){
			if(field->name() == name){
				break;
			}
			
			internalOffset += getSizeOf(*field);
		}

		return internalOffset;
	}

	int getSizeOf(blender_blend_t::dna_field_t &field){
		int size;

		// TODO: Extract this from the SDNA block.

		if(field.name()[0] == '*'){
			size = 8;
		} else if(field.type() == "char"){
			size = 1;
		} else if(field.type() == "short"){
			size = 2;
		} else if(field.type() == "int"){
			size = 4;
		} else if(field.type() == "float"){
			size = 4;
		} else {
			throw std::runtime_error(std::string("Unhandled type: ") + field.type());
		}

		size *= getArrayLength(field.name());
		
		return size;
	}
};

class DataBlock {
	private:
	std::unique_ptr<DataSource> dataSource;
	public:
	std::unique_ptr<DataPart> part;
	DataBlock(blender_blend_t &data, std::string code){
		for(auto &block : *data.blocks()){
			if(block->code() != code){
				continue;
			}

			auto &type = *data.sdna_structs()->at(block->sdna_index());
			dataSource = std::unique_ptr<DataSource>(new DataSource(block->_raw_body()));

			part = std::unique_ptr<DataPart>(new DataPart(data, *dataSource, 0, type));
		}
	}
};

int main(int argc, char **argv) {
	std::vector<std::string> arguments;
	for(int i = 0; i < argc; i++){
		arguments.push_back(argv[i]);
	}

	std::ifstream is("/home/bjorn/Desktop/blender-convert/cube.blend", std::ifstream::binary);
	kaitai::kstream ks(&is);
	blender_blend_t data(&ks);

	if(arguments.size() == 2 && arguments.at(1) == "--help"){
		printf("Usage:\n");
		printf("  blender-convert [file] --list-header          // lists file header\n");
		printf("  blender-convert [file] --list-blocks          // lists all data blocks\n");
		printf("  blender-convert [file] --list-block-with-code // lists a data block by code\n");
		printf("  blender-convert [file] --list-types           // lists all types\n");
		printf("  blender-convert [file] --list-structs         // lists all structs\n");
		printf("  blender-convert [file] --list-struct [type]   // lists a specific struct\n");

		return 0;
	}
	if(arguments.size() == 2 && arguments.at(1) == "--list-blocks"){
		int i = 0;
		for(auto &block : *data.blocks()){
			printf("%i: %s \n", i++, block->code().c_str());
		}

		return 0;
	}
	if(arguments.size() == 2 && arguments.at(1) == "--list-header"){
		auto header = data.hdr();

		printf("Blender version: %s \n", header->version().c_str());
		printf("Pointer size: %i \n", (int)header->psize());
		printf("Endianness: %s \n", header->endian() == blender_blend_t::ENDIAN_BE ? "BE" : "LE");

		return 0;
	}
	if(arguments.size() == 3 && arguments.at(1) == "--list-block-with-code"){
		int i = 0;
		std::string code = arguments.at(2);
		for(auto &block : *data.blocks()){
			if(block->code() != code){
				i++;
				continue;
			}
			printf("%i: %s\n", i, block->code().c_str());
			return 0;
		}

		printf("Not found\n");

		return 0;
	}
	if(arguments.size() == 2 && arguments.at(1) == "--list-types"){
		for(auto &block : *data.blocks()){
			if(block->code() != "DNA1"){
				continue;
			}

			auto &body = *block->body();

			for(int i = 0; i < body.num_types(); i++){
				printf("%s (%i)\n", body.types()->at(i).c_str(), body.lengths()->at(i));
			}
		}

		return 0;
	}
	if(arguments.size() == 2 && arguments.at(1) == "--list-structs"){
		for(auto &sdna_struct : *data.sdna_structs()){
			printf("%s\n", sdna_struct->type().c_str());
			for(auto &field : *sdna_struct->fields()){
				printf("  %s (%s)\n", field->name().c_str(), field->type().c_str());
			}
			printf("\n");
		}

		return 0;
	}
	if(arguments.size() == 3 && arguments.at(1) == "--list-struct"){
		for(auto &sdna_struct : *data.sdna_structs()){
			if(sdna_struct->type() != arguments.at(2)){
				continue;
			}

			printf("%s\n", sdna_struct->type().c_str());
			for(auto &field : *sdna_struct->fields()){
				printf("  %s (%s)\n", field->name().c_str(), field->type().c_str());
			}
			printf("\n");
			return 0;
		}

		printf("Not found\n");

		return 0;
	}

	//

	std::unique_ptr<DataBlock> mesh = std::unique_ptr<DataBlock>(new DataBlock(data, std::string("ME\0\0", 4)));



	printf("Converting mesh: %s\n", mesh->part->getPart("id")->getString("name").c_str());

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
