#include <cstdio>
#include <string.h>
#include <vector>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include<iostream>
#define ITERATION_NUM 10
#define NEIGH_WIN_SIZE 3
#define NEIGH_SIZE NEIGH_WIN_SIZE*NEIGH_WIN_SIZE

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <stdint.h>
#include <random>

using namespace std;
using namespace cv;

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
	Color(): r(255), g(0), b(0){};
	Color(color_t red, color_t green, color_t blue): r(red), g(green), b(blue){};

	static Color averageColor(const Color c1, const Color c2, const Color c3)
	{
		Color new_color((c1.r + c2.r + c3.r) / 3, (c1.g + c2.g + c3.g)/3, (c1.b + c2.b + c3.b)/3);
		return new_color;
	}

	double getDiff( const Color& respondPtr ) const
	{
		//return sqrt((double)(r - respondPtr.r) * (r - respondPtr.r) + (g - respondPtr.g) * (g - respondPtr.g) + (b - respondPtr.b) * (b - respondPtr.b));
		return abs(r - respondPtr.r) + abs(g - respondPtr.g) + abs(b - respondPtr.b);
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
	Image(cv::Mat image){
		uint8_t* pixelPtr = (uint8_t*)image.data;
		int cn = image.channels();
		width = image.rows;
		height = image.cols;

		for (int i = 0; i < image.cols; i++) {
			for (int j = 0; j < image.rows; j++) {
				Vec3b intensity = image.at<Vec3b>(j, i);
				uchar B = intensity.val[0];
				uchar G = intensity.val[1];
				uchar R = intensity.val[2];
				examplar.push_back(Color(R, G, B));
				}   
			}
		
		nb_table = new Neighbour[width * height];
		for(int y = 0; y < height; y++)
			for(int x = 0; x < width; x++)
				nb_table[y * width + x] = getNeighbour(x, y);		
	}

	Image(FILE *fin)
	{
		if(!fin) {
			fprintf(stderr, "Error reading file.");
			return;
		}
		char magic[8];
		int n_colors;
		fscanf(fin, "%s %d %d %d", magic, &width, &height, &n_colors);
		color_t r, g, b;
		while(fscanf(fin, "%c%c%c", &r, &g, &b) != EOF) {
			examplar.push_back(Color(r, g, b));
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

	Color findBestMatch(const Neighbour& nb_voxel, double& sum_energy) const {
		Neighbour nb_min;
		double min_energy = INT_MAX;
		for(int j = 0; j < height; ++j)
			for(int i = 0; i < width; ++i) {
				int index = j * width + i;
				double energy = nb_voxel.getDiff(nb_table[index]);
				if(energy < min_energy) {
					min_energy = energy;
					nb_min = nb_table[index] ;
				}
			}
		sum_energy += min_energy;
		return nb_min.getCenterColor();
	}

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
		y = y % height;
		x = x % width;
		if(y < 0) y += height;
		if(x < 0) x += width;
		int index = y * height + x;
		return examplar[index]; //toroidal
	}

	void clear()
	{
		if(nb_table) delete [] nb_table;
		nb_table = NULL;
		width = 0;
		height = 0;
	}
private:
	vector<Color> examplar;
	Neighbour *nb_table;
	int width;
	int height;
};

struct Point3D {
	Point3D(int xx, int yy, int zz) : x(xx), y(yy), z(zz) {}
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
		for(int index = 0; index < volSize * volSize * volSize; index++)
			volTex[index].setColor(rand() % 256, rand() % 256, rand() % 256);
	}

	Neighbour getNeighbor(const Point3D& p, PLANE planeType) const {
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
		assert(other.volSize == this->volSize);
		memcpy(volTex, other.volTex, sizeof(Color)*volSize*volSize*volSize);
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
	srand(time(NULL));
	VolumeHeader header;
	init_header(&header);
	strcpy_s(header.texName, "TEST.vol");
	header.numChannels = 3;
	header.volSize = 4;
	header.wrap = true;

	// Init data with write noise.
	VolTexture voltexture(header);
	voltexture.initWriteNoise();

	VolTexture temp_vol_texture(header);
//	FILE *fin = fopen("cabin_firewood.ppm", "rb");
	//Image texture2d(fin);
	const int vol_size = header.volSize;
	
	cv::Mat image = cv::imread("brickwall2.png", CV_LOAD_IMAGE_COLOR);
	if(! image.data )                              // Check for invalid input
	{
		cout <<  "Could not open or find the image" << std::endl ;
		return -1;
	}
//	cv::namedWindow( "Display window", CV_WINDOW_AUTOSIZE );// Create a window for display.
//	cv::imshow( "Display window", image );      
//	cv::waitKey(0); 	

	Image imagecv(image);

	for(int it = 0; it < ITERATION_NUM; ++it) {
		cout << "Now get into first iteration...\n";
		// Go over each voxel 
		double energy_this_iteration = 0.0f;
		for(int x = 0; x < vol_size; ++x) {
			for(int y = 0; y < vol_size; ++y) {
				for(int z = 0; z < vol_size; ++z) {
					//fprintf(stdout, "Gettting into Point(%d, %d, %d)\n", x, y, z);
					Point3D p(x, y, z);
					Neighbour Nx = voltexture.getNeighbor(p, VolTexture::X_PLANE);
					const Color Cx = imagecv.findBestMatch(Nx, energy_this_iteration);
					Neighbour Ny = voltexture.getNeighbor(p, VolTexture::Y_PLANE);
					const Color Cy = imagecv.findBestMatch(Ny, energy_this_iteration);
					Neighbour Nz = voltexture.getNeighbor(p, VolTexture::Z_PLANE);
					const Color Cz = imagecv.findBestMatch(Nz, energy_this_iteration);
					const Color final = Color::averageColor(Cx, Cy, Cz);
					temp_vol_texture.setColor(final, x, y, z);					
					//const Color random_color = imagecv.getColorReference(rand(), rand());				
					//voltexture.setColor(random_color, x, y, z);
				}				
			}
		}
		cout << "Itertion " << it <<" finished.\n";
		cout << "Energy this iteration : " << energy_this_iteration << endl;
		voltexture.copyFrom(temp_vol_texture);
	}
	
	FILE *fout = fopen("hopework.vol", "w");
	writeTOFile(fout, &header);
	voltexture.dumpToFile(fout);
//	fclose(fin);
	fclose(fout);
}