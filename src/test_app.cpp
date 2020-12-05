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
		
		printf("%s\n", type.type().c_str());
		
		for(auto &_field : *type.fields()){
			printf("  %s (%s)\n", _field->name().c_str(), _field->type().c_str());
		}

		printf("\n\n");
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
		int offset = getOffsetOf(name);
		int size = getArrayLength(name);

		stream->seek(offset * 2);
		return kaitai::kstream::bytes_to_str(stream->read_bytes(66), std::string("ASCII"));
	}

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

	int getOffsetOf(std::string name){
		int internalOffset = 0;

		for(auto &field : *type->fields()){
			if(field->name() == name){
				break;
			}

			int bracketPosition = field->name().find('[');

			if(bracketPosition != -1 && field->name().substr(0, bracketPosition) == name){
				break;
			}
			
			internalOffset += getSizeOf(*field);
		}

		return internalOffset;
	}

	int getSizeOf(blender_blend_t::dna_field_t &field){
		int size;

		if(field.name()[0] == '*'){
			size = 4;
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
	printf("Starting\n");
	std::ifstream is("/home/bjorn/Desktop/blender-convert/cube.blend", std::ifstream::binary);
	kaitai::kstream ks(&is);
	blender_blend_t data(&ks);

	printf("File blender version: %s\n", data.hdr()->version().c_str()); // => get hdr

	std::unique_ptr<DataBlock> mesh = std::unique_ptr<DataBlock>(new DataBlock(data, std::string("ME\0\0", 4)));

	printf("%s\n", mesh->part->getPart("id")->getString("name").c_str());

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
