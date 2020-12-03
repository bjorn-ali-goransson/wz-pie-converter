#include <stdio.h>
#include <fstream>
#include <kaitai/kaitaistream.h>
#include "blender_blend.h"

int main(int argc, char **argv) {
	printf("Starting\n");
	std::ifstream is("path/to/local/file.blend", std::ifstream::binary);
	kaitai::kstream ks(&is);
	blender_blend_t data(&ks);

	printf("Value: %s\n", data.hdr()->version().c_str()); // => get hdr

	return 0;
}


