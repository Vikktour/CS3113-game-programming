
#include "FlareMap.h"
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

FlareMap::FlareMap() {
	mapData = nullptr;
	mapWidth = -1;
	mapHeight = -1;
}

FlareMap::~FlareMap() {
	for(int i=0; i < mapHeight; i++) {
		delete mapData[i];
	}
	delete mapData;
}

//read header and initialize an array of array of heightxwidth (dimensions of tiles in the world)
bool FlareMap::ReadHeader(std::ifstream &stream) {
	std::string line;
	mapWidth = -1;
	mapHeight = -1;
	while(std::getline(stream, line)) {
		if(line == "") { break; }
		std::istringstream sStream(line);
		std::string key,value;
		std::getline(sStream, key, '=');//stores whatever is before the '=' deliminator and then discards the '='
		std::getline(sStream, value);//stores the rest of the text up until '\n' (i.e. stores the rest of the line) into value
		if(key == "width") {
			mapWidth = std::atoi(value.c_str());//converts string to int
		} else if(key == "height"){
			mapHeight = std::atoi(value.c_str());
		}
	}
	if(mapWidth == -1 || mapHeight == -1) {
		return false;
	} else {
		mapData = new unsigned int*[mapHeight];
		for(int i = 0; i < mapHeight; ++i) {
			mapData[i] = new unsigned int[mapWidth];//each height has an array
		}
		return true;
	}
}


//read [layer] and store the data(of sprite index) into each corresponding x,y tile position
bool FlareMap::ReadLayerData(std::ifstream &stream) {//static objects
	std::string line;
	while(getline(stream, line)) {
		if(line == "") { break; }
		std::istringstream sStream(line);
		std::string key,value;
		std::getline(sStream, key, '=');
		std::getline(sStream, value);
		if(key == "data") {//keep while-looping until you get to data(i.e. skip the 'type')
			for(int y=0; y < mapHeight; y++) {
				getline(stream, line);
				std::istringstream lineStream(line);
				std::string tile;
				for(int x=0; x < mapWidth; x++) {
					std::getline(lineStream, tile, ',');//for every data value, convert to int-1 and then store it into mapdata's matrix (array of arrays: width x length)
					int val = atoi(tile.c_str());
					if(val > 0) {
						mapData[y][x] = val-1;
					} else {//if the val = 0 then it's considered a tile of empty space
						mapData[y][x] = -1;
						//mapData[y][x] = 50;//debug
					}
				}
			}
		}
	}
	return true;
}

//store the tileblock x,y position of the dynamic entity (e.g. alligator is placed in (15blocksDown,20blocksRight) in the world so alligatorMap.x = 15, alligatorMap.y=20)
bool FlareMap::ReadEntityData(std::ifstream &stream) {//dynamic objects (can move: i.e. walk or disappear)
	std::string line;
	std::string type;
	while(getline(stream, line)) {
		if(line == "") { break; }
		std::istringstream sStream(line);
		std::string key,value;
		getline(sStream, key, '=');
		getline(sStream, value);//in this case, value will be the string indicating what kind of entity (e.g. mushrooms? turtles? speedboost powerup?)
		if(key == "type") {
			type = value;
		} else if(key == "location") {
			std::istringstream lineStream(value);//contains all the coordinate point location of the object, note it's (x,y,z,1) where z=1 and the last '1' is for a homogeneous vector of higher dimension. i.e. allows for transformation matrix to work.
			std::string xPosition, yPosition;
			getline(lineStream, xPosition, ',');
			getline(lineStream, yPosition, ',');//only take the first two values of linestream (i.e. contains the x,y coords)
			
			FlareMapEntity newEntity;//FlareMapEntity stores type,x-pos,y-pos members
			newEntity.type = type;
			newEntity.x = std::atoi(xPosition.c_str());
			newEntity.y = std::atoi(yPosition.c_str());
			entities.push_back(newEntity);//these dynanmic entities are stored into the 'entities' vector
		}
	}
	return true;
}

void FlareMap::Load(const std::string fileName) {//insert .txt file
	std::ifstream infile(fileName);
	if(infile.fail()) {
		assert(false); // unable to open file
	}
	std::string line;
	while (std::getline(infile, line)) {
		if(line == "[header]") {
			if(!ReadHeader(infile)) {
				assert(false); // invalid file data
			}
		} else if(line == "[layer]") {
			ReadLayerData(infile);
		} else if(line == "[Object Layer 1]") {
			ReadEntityData(infile);
		}
	}
}
//note that irrelevant lines like tilewidth,tileheight,orientation,background_color,etc are skipped
