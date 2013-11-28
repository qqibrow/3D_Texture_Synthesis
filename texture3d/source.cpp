#include <cstdio>
#include <string.h>
#include <vector>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#define ITERATION_NUM 1
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

typedef unsigned char color_t;


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
	Color(): r(0), g(0), b(0){};
	Color(color_t red, color_t green, color_t blue): r(red), g(green), b(blue){};

	static Color averageColor(const Color c1, const Color c2, const Color c3)
	{
		Color new_color((c1.r + c2.r + c3.r) / 3, (c1.g + c2.g + c3.g)/3, (c1.b + c2.b + c3.b)/3);
		return new_color;
	}

	double getDiff( const Color& respondPtr ) const
	{
		//throw std::exception("The method or operation is not implemented.");
		return sqrt((double)(r - respondPtr.r) * (r - respondPtr.r) + (g - respondPtr.g) * (g - respondPtr.g) + (b - respondPtr.b) * (b - respondPtr.b));
	}

	void setColor(color_t red, color_t green, color_t blue)
	{
		r = red;
		g = green;
		b = blue;
	}

	color_t r;
	color_t g;
	color_t b;
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
	Image();
	Image(FILE *fin)
	{
		if(!fin) {
			fprintf(stderr, "Error reading file.");
			return;
		}
		char magic[8];
		int n_colors;
		fscanf(fin, "%s %d %d %d", magic, &width, &height, &n_colors);
		examplar = new Color[width * height];
		color_t r, g, b;
		int index = 0;
		while(fscanf(fin, "%c%c%c", &r, &g, &b) != EOF) {
			examplar[index++].setColor(r, g, b);
		}
		nb_table = new Neighbour[width * height];
		for(int y = 0; y < height; y++)
			for(int x = 0; x < width; x++)
				nb_table[y * width + x] = getNeighbour(x, y);
	}

	~Image()
	{
		clear();
	}

	Color findBestMatch(const Neighbour& nb_voxel) const {
		Neighbour nb_min;
		double min_energy = INT_MAX;
		for(int j = 0; j < height; ++j)
			for(int i = 0; i < width; ++i) {
				//Neighbour nb_image = getNeighbour(i, j);
				int index = j * width + i;
				double energy = nb_voxel.getDiff(nb_table[index]);
				if(energy < min_energy) {
					min_energy = energy;
					nb_min = nb_table[index] ;
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

	const Color& getColorReference(int x, int y) const
	{
		if(y < 0) y += height;
		if(x < 0) x += width;
		return examplar[(y % height) * width + (x % width)]; //toroidal
	}

	void clear()
	{
		//delete [] examplar;
		delete [] nb_table;
		width = 0;
		height = 0;
	}

	Color *examplar;
	Neighbour *nb_table;
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
	VolTexture(const VolumeHeader& header) //*
	{
		volSize = header.volSize;
		volTex = new Color[volSize * volSize * volSize];
	}

	~VolTexture()
	{
		clear();
	}

	void initWriteNoise() 
	{
		srand(time(NULL));
		for(int index = 0; index < volSize * volSize * volSize; index++)
			volTex[index].setColor(rand() % 256, rand() % 256, rand() % 256);
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
			break;
		case Z_PLANE:
			zboundary.left = zboundary.right = 0;
			break;
		}
		Neighbour nb;
		for(int x = xboundry.left; x <= xboundry.right; ++x)
			for(int y = ybondary.left; y <= ybondary.right; ++y)
				for(int z = zboundary.left; z <= zboundary.right; ++z) {
					int index = getIndex(x, y, z);
					nb.add(getColorReference(x, y, z));
				}
		return nb;
	}
	void setColor(const Color &color, int x, int y, int z)
	{
		volTex[getIndex(x, y, z)].setColor(color.r, color.g, color.b);
	}
	void copyFrom(const VolTexture& other)
	{
		clear();
		volSize = other.getSize();
		volTex = new Color[volSize * volSize * volSize];
		for(int x = 0; x < volSize; x++)
			for(int y = 0; y < volSize; y++)
				for(int z = 0; z < volSize; z++) {
					setColor(other.getColorReference(x, y, z), x, y, z);
				}
	}
	int getIndex(int x, int y, int z) const
	{
		if(x < 0) x += volSize;
		if(y < 0) y += volSize;
		if(z < 0) z += volSize;
		return ((y % volSize) * volSize + (x % volSize)) * volSize + (z % volSize); //toroidal
	}
	int getSize() const
	{
		return volSize;
	}
	void dumpToFile(FILE *stream) const
	{
		for(int index = 0; index < volSize * volSize * volSize; index++)
		{
			fprintf(stream, "%c%c%c", volTex[index].r, volTex[index].g,volTex[index].b);
		}
	}
private:
	const Color& getColorReference(int x, int y, int z) const
	{
		return volTex[getIndex(x, y, z)];
	}
	void clear()
	{
		volSize = 0;
		delete [] volTex;
	}
	int volSize;
	Color *volTex;
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

	VolTexture temp_vol_texture(header);
	FILE *fin = fopen("cabin_firewood.ppm", "rb");
	Image texture2d(fin);
	const int vol_size = header.volSize;
	
	for(int it = 0; it < ITERATION_NUM; ++it) {

		// Go over each voxel 
		for(int x = 0; x < vol_size; ++x) {
			for(int y = 0; y < vol_size; ++y) {
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
				
			}
			printf("%d\n", x);
		}
		voltexture.copyFrom(temp_vol_texture);
	}
	
	FILE *fout = fopen("new_vol.vol", "w");
	writeTOFile(fout, &header);
	voltexture.dumpToFile(fout);
	fclose(fin);
	fclose(fout);
}