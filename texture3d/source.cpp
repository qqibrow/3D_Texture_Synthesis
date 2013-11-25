#include <cstdio>
#include <string.h>

#define ITERATION_NUM 2

using namespace std;

struct VolumeHeader
{
	char magic[4];
	int version;
	char texName[256];
	bool wrap;
	int volSize;
	int numChannels;
	int bytesPerChannel;
};

void writeTOFile(FILE* stream, VolumeHeader* header) {
	char buf[4096];
	memcpy(buf, header, sizeof(VolumeHeader));
	fwrite (buf , sizeof(char), sizeof(buf), stream);
}

void init_header(VolumeHeader* header) {
	const char* magic = "VOLU";
	memcpy(header->magic, magic, 4 * sizeof(char));
	header->version = 4;
	header->bytesPerChannel = 1;
}

int readTestCode() {
	FILE * fin = fopen("woodwall.vol", "rb");
	if(!fin) {
		fprintf(stderr, "Error reading file.");
		return -1;
	}
	char buf[4096];
	fread(buf, 4096, 1, fin);

	VolumeHeader * header = (VolumeHeader *)buf;
	if ((header->magic[0] != 'V') || (header->magic[1] != 'O') || (header->magic[2] != 'L') || (header->magic[3] != 'U')){
		fprintf(stderr, "bad header: invalid magic");
		fclose(fin);
		return -1;
	}

	if (header->version != 4)
		fprintf(stderr, "bad header: version != 4");
	if (header->bytesPerChannel != 1)
		fprintf(stderr, "bad header: only byte textures supported");

	const int volSize = header->volSize;
	const int numChannels = header->numChannels;
	int volBytes = header->volSize*header->volSize*header->volSize*header->numChannels;
	unsigned char * data = new unsigned char[volBytes];
	fread(data, volBytes, 1, fin);

	fclose(fin);
}

struct Color{
	static Color averageColor(const Color c1, const Color c2, const Color c3);
};

struct Neighbour{

};

struct Image {
public:
	Color findBestMatch(const Neighbour& neighbor) const;
	
};

struct Point {
	Point(int xx, int yy, int zz) : x(xx), y(yy), z(zz) {}
	int x, y, z;
};

class VolTexture {
public:
	enum PLANE {X_PLANE, Y_PLANE, Z_PLANE};
	VolTexture();
	VolTexture(const VolumeHeader& header);
	Neighbour getNeighbor(const Point& p, PLANE planeType) const;
	void setColor(const Color color, int x, int y, int z);
	void copyFrom(const VolTexture& other);
};


int main() {
	VolumeHeader header;
	init_header(&header);
	strcpy(header.texName, "TEST.vol");
	header.numChannels = 3;
	header.volSize = 128;
	header.wrap = true;

	// Init data with write noise.
	VolTexture voltexture(header);




	VolTexture temp_vol_texture;

	Image texture2d;
	const int vol_size = header.volSize;
	for(int it = 0; it < ITERATION_NUM; ++it) {

		// Go over each voxel 
		for(int x = 0; x < vol_size; ++x)
			for(int y = 0; y < vol_size; ++y)
				for(int z = 0; z < vol_size; ++z) {
					Point p(x, y, z);
					Neighbour Nx = voltexture.getNeighbor(p, VolTexture::X_PLANE);
					const Color Cx = texture2d.findBestMatch(Nx);
					Neighbour Ny = voltexture.getNeighbor(p, VolTexture::Y_PLANE);
					const Color Cy = texture2d.findBestMatch(Ny);
					Neighbour Nz = voltexture.getNeighbor(p, VolTexture::Z_PLANE);
					const Color Cz = texture2d.findBestMatch(Nz);
					const Color final = Color::averageColor(Cx, Cy, Cz);
					temp_vol_texture.setColor(final, x, y, z);
				}
		voltexture.copyFrom(temp_vol_texture);
	}	
}