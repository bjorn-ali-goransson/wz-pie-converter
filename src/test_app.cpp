#include <stdio.h>
#include <fstream>
#include <kaitai/kaitaistream.h>
#include "blender_blend.h"
#include <map>
#include <algorithm>

// References:
//   https://formats.kaitai.io/blender_blend/index.html
//   http://homac.cakelab.org/projects/JavaBlend/spec.html
//   https://developer.blender.org/diffusion/B/browse/master/doc/blender_file_format/BlendFileReader.py
//   https://fossies.org/linux/blender/doc/blender_file_format/mystery_of_the_blend.html
//   https://archive.blender.org/wiki/index.php/Dev:Source/Architecture/File_Format/#Structure_DNA
//   https://wiki.blender.org/wiki/Source/Architecture/RNA

unsigned long long readPointer(const char* data, int pointerSize) {
	unsigned long long int value = 0;

	auto small = static_cast<unsigned long int>(
		static_cast<unsigned char>(data[0]) << 24 |
		static_cast<unsigned char>(data[1]) << 16 | 
		static_cast<unsigned char>(data[2]) << 8  | 
		static_cast<unsigned char>(data[3])
	);

	value = small;

	if(pointerSize == 8){
		auto big = static_cast<unsigned long int>(
			static_cast<unsigned char>(data[4]) << 24 |
			static_cast<unsigned char>(data[5]) << 16 | 
			static_cast<unsigned char>(data[6]) << 8  | 
			static_cast<unsigned char>(data[7])
		);

		value = big << 32 | value;
	}

	return value;
};

class BlendField{
	public:
	std::string name;
	std::string type;
	int size;
	int offset;
	int arraySize = -1;
	BlendField(std::string name, std::string type, int size, int offset, int arraySize){
		this->name = name;
		this->type = type;
		this->size = size;
		this->offset = offset;
		this->arraySize = arraySize;
	}
};

class BlendType {
	private:
	std::vector<std::unique_ptr<BlendField>> fields;
	std::map<std::string, BlendField*> fieldsByName;

	public:
	std::string name;
	int size;
	BlendType(std::string name, int size, std::vector<BlendField*> fields){
		this->name = name;
		this->size = size;

		for(auto field : fields){
			this->fields.push_back(std::unique_ptr<BlendField>(field));

			this->fieldsByName[field->name] = field;
		}
	}

	std::vector<BlendField*> getFields(){
		std::vector<BlendField*> result;
		for(auto &field : fields){
			result.push_back(&*field);
		}
		return result;
	}

	BlendField* getField(std::string name){
		if(!fieldsByName.count(name)){
			throw std::runtime_error(std::string("Could not find field ") + name + " on type " + this->name);
		}

		return fieldsByName.at(name);
	}
};

class TypeProvider {
	private:
	blender_blend_t::dna1_body_t *dna;
	std::map<std::string, BlendType*> typesByName;
	std::map<int, std::string> typesBySdnaIndex;
	std::map<std::string, int> sdnaIndexesByType;
	std::map<std::string, int> typeLengths;

	public:
	int pointerSize;
	TypeProvider(blender_blend_t &data){
		pointerSize = data.hdr()->psize();

		for(auto &block : *data.blocks()){
			if(block->code() != "DNA1"){
				continue;
			}

			dna = &*block->body();

			for(int i = 0; i < dna->num_structs(); i++){
				auto type = &*dna->structs()->at(i);
				
				typesByName[type->type()] = nullptr;
				typesBySdnaIndex[i] = type->type();
				sdnaIndexesByType[type->type()] = i;
			}

			for(int i = 0; i < dna->num_types(); i++){
				auto name = dna->types()->at(i);
				auto typeLength = dna->lengths()->at(i);

				typeLengths[name] = typeLength;
			}
		}
	}

	int getTypeLength(std::string name){
		if(!typeLengths.count(name)){
			throw std::runtime_error(std::string("Could not find type ") + name);
		}

		return typeLengths[name];
	}

	BlendType* getType(int sdnaIndex){
		if(!typesBySdnaIndex.count(sdnaIndex)){
			char data[100];
			sprintf(data, "Could not find type with SDNA index %i", sdnaIndex);
			throw std::runtime_error(std::string(data));
		}

		auto name = typesBySdnaIndex.at(sdnaIndex);

		return getType(name);
	}

	BlendType* getType(std::string name){
		if(!typesByName.count(name)){
			throw std::runtime_error(std::string("Could not find type ") + name);
		}

		if(typesByName.at(name) != nullptr){
			return typesByName.at(name);
		}

		int index = sdnaIndexesByType.at(name);

		auto sdna_struct = &*dna->structs()->at(index);

		std::vector<BlendField*> fields;
		int offset = 0;
		for(auto &field : *sdna_struct->fields()){
			auto fieldName = field->name();
			auto fieldType = field->type();
			int size;
			int arraySize = 1;

			if(fieldName[0] == '*'){
				size = pointerSize;
			} else {
				size = typeLengths[fieldType];
			}

			int bracketPosition = fieldName.find('[');

			if(bracketPosition != -1){
				int bracketEnd = fieldName.find(']');

				if(bracketEnd == -1){
					throw std::runtime_error(std::string("Array syntax was not name[length] - no end bracket"));
				}

				arraySize = std::stoi(fieldName.substr(bracketPosition + 1, bracketEnd - bracketPosition));

				if(bracketEnd != fieldName.length() - 1){
					arraySize = 8*3; // multidimensional arrays are hardcoded as being [x][3] for now.
					//throw std::runtime_error(std::string("Array syntax was not name[length] - something came after last bracket"));
				}


				fieldName = fieldName.substr(0, bracketPosition);
			}

			size *= arraySize;

			fields.push_back(new BlendField(fieldName, fieldType, size, offset, arraySize));

			offset += size;
		}

		auto type = new BlendType(name, offset, fields);

		typesByName[type->name] = type;

		return type;
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
	DataSource *dataSource;
	unsigned long long blockPosition;
	size_t offset;
	std::unique_ptr<kaitai::kstream> stream;

	public:
	BlendType *type;
	DataPart(TypeProvider *typeProvider, DataSource *dataSource, unsigned long long blockPosition, size_t offset, BlendType *type){
		this->typeProvider = typeProvider;
		this->dataSource = dataSource;
		this->blockPosition = blockPosition;
		this->offset = offset;
		this->stream = dataSource->getStream(offset);
		this->type = type;
	}

	std::unique_ptr<DataPart> getPart(std::string name){
		auto field = type->getField(name);
		auto type = typeProvider->getType(field->type);

		return std::unique_ptr<DataPart>(new DataPart(typeProvider, dataSource, blockPosition, field->offset, type));
	}

	int32_t getInt(std::string name){
		auto field = type->getField(name);

		stream->seek(offset + field->offset);

		if(this->typeProvider->getTypeLength("int") != 4){
			char data[100];
			sprintf(data, "Int sizes other than 4 bytes are not supported (was %i)", this->typeProvider->getTypeLength("int"));
			throw std::runtime_error(std::string(data));
		}

		return stream->read_s4le();
	}

	float getFloat(std::string name, unsigned int arrayIndex = 0){
		auto field = type->getField(name);

		stream->seek(offset + field->offset + arrayIndex * this->typeProvider->getTypeLength("float"));

		if(this->typeProvider->getTypeLength("float") != 4){
			char data[100];
			sprintf(data, "Float sizes other than 4 bytes are not supported (was %i)", this->typeProvider->getTypeLength("float"));
			throw std::runtime_error(std::string(data));
		}

		return stream->read_f4le();
	}

	std::string getString(std::string name){
		auto field = type->getField(name);

		stream->seek(offset + field->offset);
		return kaitai::kstream::bytes_to_str(stream->read_bytes(field->size), std::string("ASCII"));
	}

	unsigned long long getPointer(std::string name){
		auto field = type->getField(name);

		stream->seek(offset + field->offset);
		return readPointer(stream->read_bytes(typeProvider->pointerSize).c_str(), typeProvider->pointerSize);
	}
};

class DataBlock {
	public:
	std::unique_ptr<DataSource> dataSource;
	std::unique_ptr<DataPart> part;
	int index;
	std::string code;
	unsigned long long memaddr;
	DataBlock(DataSource *dataSource, DataPart *part, unsigned int index, std::string code, unsigned long long memaddr){
		this->dataSource = std::unique_ptr<DataSource>(dataSource);
		this->part = std::unique_ptr<DataPart>(part);
		this->index = index;
		this->code = code;
		this->memaddr = memaddr;
	}
};

class BlockItem {
	public:
	unsigned long long position;
	unsigned int length;
	unsigned int index;
	std::string code;
	blender_blend_t::file_block_t *block;

	BlockItem(unsigned long long position, unsigned int length, unsigned int index, std::string code, blender_blend_t::file_block_t *block){
		this->position = position;
		this->length = length;
		this->index = index;
		this->code = code;
		this->block = block;
	}
};

bool blockItemComparer (const std::unique_ptr<BlockItem> &a, const std::unique_ptr<BlockItem> &b) {
	return a->position < b->position;
}

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

	std::unique_ptr<DataBlock> getBlock(unsigned long long pointer){
		int candidates = 0;

		auto blocks = std::vector<std::unique_ptr<BlockItem>>();

		unsigned int index;
		for(auto &block : *data->blocks()){
			auto position = readPointer(block->mem_addr().c_str(), pointerSize);

			blocks.push_back(std::unique_ptr<BlockItem>(new BlockItem(position, block->len_body(), index++, std::string(block->code()), &*block)));
		}

		std::sort (blocks.begin(), blocks.end(), blockItemComparer);

		BlockItem *item = nullptr;

		for(const auto &block : blocks){
			if(block->position == 0){
				continue; // ENDB block does that, empty markers to signify EOF.
			}

			auto blockEnd = block->position + block->length;

			if(pointer < block->position){
				continue;
			}
			if(pointer > blockEnd){
				continue;
			}

			item = &*block;
			candidates++;
		}

		if(candidates > 1){
			char data[100];
			sprintf(data, "Ambigious pointer reference - 0x%08llx resolves to multiple blocks", pointer);
			throw std::runtime_error(std::string(data));
		}


		auto type = typeProvider->getType(item->block->sdna_index());
		auto dataSource = new DataSource(item->block->_raw_body());
		auto part = new DataPart(typeProvider, dataSource, item->position, 0, type);

		return std::unique_ptr<DataBlock>(new DataBlock(dataSource, part, item->index, item->code, item->position));
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

			auto position = readPointer(block->mem_addr().c_str(), pointerSize);

			auto type = typeProvider->getType(block->sdna_index());
			auto dataSource = new DataSource(block->_raw_body());
			auto part = new DataPart(typeProvider, dataSource, position, 0, type);

			return std::unique_ptr<DataBlock>(new DataBlock(dataSource, part, index, std::string(block->code()), position));
		}

		throw std::runtime_error(std::string("Could not find block ") + code);
	}
};

class PointedDataProvider {
	private:
	TypeProvider *typeProvider;
	BlockProvider *blockProvider;
	public:
	PointedDataProvider(TypeProvider *typeProvider, BlockProvider *blockProvider){
		this->typeProvider = typeProvider;
		this->blockProvider = blockProvider;
	}
	std::unique_ptr<DataPart> getPointedData(DataPart *dataPart, std::string name, unsigned int arrayIndex = 0){
		auto pointer = dataPart->getPointer(name);
		auto block = blockProvider->getBlock(pointer);
		auto field = dataPart->type->getField(name);
		auto fieldType = typeProvider->getType(field->type);

		return std::unique_ptr<DataPart>(new DataPart(typeProvider, &*block->dataSource, block->memaddr, pointer - block->memaddr + fieldType->size * arrayIndex, fieldType));
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
	PointedDataProvider pointedDataProvider(&typeProvider, &blockProvider);

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

		printf("[%i] 0x%08llx : %s\n", block->index, block->memaddr, block->code.c_str());
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

	auto vertexCount = mesh->part->getInt("totvert");
	printf("Total vertices: %i\n", vertexCount);
	auto polygonCount = mesh->part->getInt("totpoly");
	printf("Total polys: %i\n", polygonCount);
	printf("Total loops: %i\n", mesh->part->getInt("totloop"));

	printf("Polygons:\n");

	for(int i = 0; i < polygonCount; i++){
		auto mpoly = pointedDataProvider.getPointedData(&*mesh->part, "*mpoly", i);

		printf("Loop start: %i\n", mpoly->getInt("loopstart"));
		printf("Loop length: %i\n", mpoly->getInt("totloop"));
	}

	// printf("Vertices:\n");
	// for(int i = 0; i < vertexCount; i++){
	// 	if(i){
	// 		printf("----------\n");
	// 	}
	// 	auto mvert = pointedDataProvider.getPointedData(&*mesh->part, "*mvert", i);
	// 	printf("  Vertex:\n");
	// 	printf("    X: %0.10f\n", mvert->getFloat("co", 0));
	// 	printf("    Y: %0.10f\n", mvert->getFloat("co", 1));
	// 	printf("    Z: %0.10f\n", mvert->getFloat("co", 2));
	// 	printf("  Normal:\n");
	// 	printf("    X: %0.10f\n", mvert->getFloat("no", 0));
	// 	printf("    Y: %0.10f\n", mvert->getFloat("no", 1));
	// 	printf("    Z: %0.10f\n", mvert->getFloat("no", 2));
	// dont forget normals in here!! "no"
	// }

	return 0;
}
