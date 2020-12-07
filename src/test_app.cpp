#include <stdio.h>
#include <fstream>
#include <kaitai/kaitaistream.h>
#include "blender_blend.h"
#include <map>

// References:
//   https://formats.kaitai.io/blender_blend/index.html
//   http://homac.cakelab.org/projects/JavaBlend/spec.html
//   https://developer.blender.org/diffusion/B/browse/master/doc/blender_file_format/BlendFileReader.py
//   https://fossies.org/linux/blender/doc/blender_file_format/mystery_of_the_blend.html
//   https://archive.blender.org/wiki/index.php/Dev:Source/Architecture/File_Format/#Structure_DNA
//   https://wiki.blender.org/wiki/Source/Architecture/RNA

class Pointer {
	private:
	unsigned long int small = 0;
	unsigned long int big = 0;

	public:
	unsigned long long int value = 0;
	Pointer(const char* data, int pointerSize){
		small = static_cast<unsigned int>(
			static_cast<unsigned char>(data[0]) << 24 |
			static_cast<unsigned char>(data[1]) << 16 | 
			static_cast<unsigned char>(data[2]) << 8  | 
			static_cast<unsigned char>(data[3])
		);

		value = small;

		if(pointerSize == 8){
			big = static_cast<unsigned int>(
				static_cast<unsigned char>(data[4]) << 24 |
				static_cast<unsigned char>(data[5]) << 16 | 
				static_cast<unsigned char>(data[6]) << 8  | 
				static_cast<unsigned char>(data[7])
			);

			value = big << 32 | value;
		}
	}

	std::string toString(){
		char buff[18];
		snprintf(buff, sizeof(buff), "0x%04x%04x", big, small);
		return buff;
	}
};

class BlendType {
	public:
	std::string name;
	int size;
	BlendType(std::string name, int size){
		this->name = name;
		this->size = size;
	}
};

class TypeProvider {
	private:
	blender_blend_t *data;
	std::vector<std::unique_ptr<BlendType>> types;
	std::map<std::string, BlendType*> typesByName;

	public:
	int pointerSize;
	TypeProvider(blender_blend_t &data){
		this->data = &data;
		pointerSize = data.hdr()->psize();

		for(auto &block : *data.blocks()){
			if(block->code() != "DNA1"){
				continue;
			}

			auto &body = *block->body();

			for(int i = 0; i < body.num_types(); i++){
				auto type = new BlendType(body.types()->at(i), body.lengths()->at(i));
				types.push_back(std::unique_ptr<BlendType>(type));
				typesByName.insert(std::make_pair(type->name, type));
			}
		}
	}

	BlendType* getType(std::string name){
		if(!typesByName.count(name)){
			return nullptr;
		}

		return typesByName.at(name);
	}
};

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
	TypeProvider *typeProvider;
	blender_blend_t *data;
	DataSource *dataSource;
	size_t offset;
	std::unique_ptr<kaitai::kstream> stream;
	blender_blend_t::dna_struct_t *type;

	public:
	DataPart(TypeProvider *typeProvider, blender_blend_t *data, DataSource *dataSource, size_t pos, blender_blend_t::dna_struct_t *type){
		this->typeProvider = typeProvider;
		this->data = data;
		this->dataSource = dataSource;
		this->offset = pos;
		this->stream = dataSource->getStream(pos);
		this->type = type;
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

			return std::unique_ptr<DataPart>(new DataPart(typeProvider, data, dataSource, offset, fieldType));
		}

		return nullptr;
	}

	int32_t getInt(std::string name){
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

		stream->seek(offset);
		return stream->read_s4le();
	}

	std::unique_ptr<Pointer> getPointer(std::string name){
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

		stream->seek(offset);
		return std::unique_ptr<Pointer>(new Pointer(stream->read_bytes(typeProvider->pointerSize).c_str(), typeProvider->pointerSize));
	}

	std::unique_ptr<DataPart> getPointedData(std::string name){
		auto pointer = getPointer(name);

		return std::unique_ptr<DataPart>(new DataPart(typeProvider, data, dataSource, pointer->value - blockOffset, fieldType));
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
		int size = -1;

		if(field.name()[0] == '*'){
			size = 8;
		} else {
			auto type = typeProvider->getType(field.type());

			if(type != nullptr){
				size = type->size;
			}
		}

		if(size == -1) {
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
	int index;
	std::string code;
	std::unique_ptr<Pointer> memaddr;
	DataBlock(TypeProvider *typeProvider, blender_blend_t *data, int index, std::string code, Pointer *memaddr){
		this->index = index;
		this->code = code;
		this->memaddr = std::unique_ptr<Pointer>(memaddr);

		for(auto &block : *data->blocks()){
			if(block->code() != code){
				continue;
			}

			auto type = &*data->sdna_structs()->at(block->sdna_index());
			dataSource = std::unique_ptr<DataSource>(new DataSource(block->_raw_body()));

			part = std::unique_ptr<DataPart>(new DataPart(typeProvider, data, &*dataSource, 0, type));
		}
	}
};

class BlockProvider {
	private:
	TypeProvider *typeProvider;
	blender_blend_t *data;

	public:
	int pointerSize;
	BlockProvider(TypeProvider *typeProvider, blender_blend_t &data){
		this->typeProvider = typeProvider;
		this->data = &data;
		pointerSize = data.hdr()->psize();
	}

	std::unique_ptr<DataBlock> getBlock(std::string code){
		if(code.length() == 2){
			char codeChars[4] = {
				code[0],
				code[1],
				'\0',
				'\0',
			};
			code = std::string(codeChars, 4);
		}
		if(code.length() == 3){
			char codeChars[4] = {
				code[0],
				code[1],
				code[2],
				'\0',
			};
			code = std::string(codeChars, 4);
		}

		int index;

		for(auto &block : *data->blocks()){
			if(block->code() != code){
				index++;
				continue;
			}

			auto &body = *block->body();

			Pointer *memaddr = new Pointer(block->mem_addr().c_str(), pointerSize);

			return std::unique_ptr<DataBlock>(new DataBlock(typeProvider, data, index, code, memaddr));
		}

		throw std::runtime_error(std::string("Could not find block ") + code);
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

	TypeProvider typeProvider(data);
	BlockProvider blockProvider(&typeProvider, data);

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
		std::string code = arguments.at(2);

		auto block = blockProvider.getBlock(code);

		if(block == nullptr){
			printf("Not found\n");
			return 0;
		}

		printf("[%i] %s : %s\n", block->index, block->memaddr->toString().c_str(), block->code.c_str());
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

	std::unique_ptr<DataBlock> mesh = blockProvider.getBlock("ME");

	printf("Converting mesh: %s\n", mesh->part->getPart("id")->getString("name").c_str());

	printf("Mesh block position: %s\n", mesh->memaddr->toString().c_str());
	printf("Total vertices: %s\n", mesh->part->getPointedPart("**mat")->getPart("id")->getString("name").c_str());

	return 0;
}
