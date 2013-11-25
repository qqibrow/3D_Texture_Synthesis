#include <cstdio>
#include <string.h>
#include <vector>
#include <assert.h>
#define ITERATION_NUM 2
#define NEIGH_WIN_SIZE 3
#define NEIGH_SIZE NEIGH_WIN_SIZE*NEIGH_WIN_SIZE
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

	double getDiff( const Color& respondPtr ) const
	{
		throw std::exception("The method or operation is not implemented.");
	}


};

struct Boundary{
	Boundary(int l, int r): left(l), right(r) {}
	int left, right;
};

struct Neighbour{
	Neighbour():colors(NEIGH_SIZE, NULL), size(0){};
	bool add(const Color& color) {
		if(size == NEIGH_SIZE)
			return false;
		colors[size++] = &color;
		return true;
	}

	double getDiff( const Neighbour& nb_image ) const
	{
		double diff = 0.0f;
		for(int i = 0; i < NEIGH_SIZE; ++i) {
			const Color* colorptr = colors[i];
			assert(colorptr);
			const Color* respondPtr = nb_image.getColorPtr(i);
			assert(respondPtr);
			diff += colorptr->getDiff(*respondPtr);
		}
		return diff;
	}

	Color getCenterColor() const
	{	
		const Color* ptr = colors[ colors.size() / 2 ];
		return *ptr;
	}

	const Color* getColorPtr( int i ) const
	{
		assert(i >= 0 && i < colors.size() && "index not correct.");
		return colors[i];
	}

private:
	vector<const Color*> colors;
	int size;
};

struct Image {
public:
	Color findBestMatch(const Neighbour& nb_voxel) const {
		Neighbour nb_min;
		double min_energy = INT_MAX;
		for(int j = 0; j < height; ++j)
			for(int i = 0; i < width; ++i) {
				Neighbour nb_image = getNeighbour(i, j);
				double energy = nb_voxel.getDiff(nb_image);
				if(energy < min_energy) {
					min_energy = energy;
					nb_min = nb_image;
				}
			}
		return nb_min.getCenterColor();
	}
private:
	Neighbour getNeighbour(int i, int j) const {
		const int half_win_size = (NEIGH_WIN_SIZE - 1) / 2;
		Boundary xboundary(i - half_win_size, i + half_win_size),
			yboundary(j - half_win_size, j + half_win_size);

		Neighbour nb;
		for(int x = xboundary.left; x <= xboundary.right; ++x)
			for(int y = yboundary.left; y <= yboundary.right; ++y) {
				nb.add(getColorReference(x, y));
			}
		return nb;
	}

	const Color& getColorReference(int x, int y) const;
	int width;
	int height;
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

	void initWriteNoise() 
	{
		throw std::exception("The method or operation is not implemented.");
	}

	Neighbour getNeighbor(const Point& p, PLANE planeType) const {
		const int half_win_size = (NEIGH_WIN_SIZE - 1) / 2;
		Boundary xboundry(p.x - half_win_size, p.x + half_win_size), 
			ybondary(p.y - half_win_size, p.y + half_win_size),
			zboundary(p.z - half_win_size, p.z + half_win_size);

		switch(planeType) {
		case X_PLANE:
			xboundry.left = xboundry.right = 0;
			break;
		case Y_PLANE:
			ybondary.left = ybondary.right = 0;
		case Z_PLANE:
			zboundary.left = zboundary.right = 0;
		}
		Neighbour nb;
		for(int x = xboundry.left; x <= xboundry.right; ++x)
			for(int y = ybondary.left; y <= ybondary.right; ++y)
				for(int z = zboundary.left; z <= zboundary.right; ++z) {
					nb.add(getColorReference(x, y, z));
				}
		return nb;
	}
	void setColor(const Color color, int x, int y, int z);
	void copyFrom(const VolTexture& other);
private:
	const Color& getColorReference(int x, int y, int z) const; 
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
	voltexture.initWriteNoise();

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