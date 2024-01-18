#include <iostream>
#include "include/slicer.h"
#include "include/MeshGroup.h"
#include <qfile.h>
#include "clipper.hpp"

#include <chrono>

using namespace cura;
using namespace std::chrono;

bool writeCLIHeader(QFile* file, std::vector<SlicerLayer>& layers, float unit)
{
	QString strHeader;
	strHeader += QString("$$HEADERSTART\n");
	strHeader += "$$BINARY\n";
	strHeader += QString("$$UNITS/%1\n").arg(unit, 15, 'f', 6, QLatin1Char('0'));

	float maxPos[3], minPos[3];
	maxPos[0] = maxPos[1] = maxPos[2] = FLT_MIN;
	minPos[0] = minPos[1] = minPos[2] = FLT_MAX;
	// update bbox of model
	bool isFirstLayer = false;
	for (int i = 0; i < layers.size(); ++i) {
		const SlicerLayer& layer = layers[i];
		if (!isFirstLayer) {
			minPos[2] = layer.z;
			isFirstLayer = true;
		}

		const ClipperLib::Paths& paths = layer.polygons.paths;
		for (int j = 0; j < paths.size(); j++) {
			ClipperLib::Path path = paths[j];

			for (int k = 0; k < path.size(); ++k) {
				float x = INT2MM(path[k].X) / unit;
				float y = INT2MM(path[k].Y) / unit;
				maxPos[0] = x > maxPos[0] ? x : maxPos[0];
				maxPos[1] = y > maxPos[1] ? y : maxPos[1];
				minPos[0] = x < minPos[0] ? x : minPos[0];
				minPos[1] = y < minPos[1] ? y : minPos[1];
			}
		}
	}
	maxPos[2] = layers.back().z;

	QString strMax = QString("%1,%2,%3").arg(maxPos[0], 15, 'f', 6, QLatin1Char('0')).arg(maxPos[1], 15, 'f', 6, QLatin1Char('0')).arg(maxPos[2], 15, 'f', 6, QLatin1Char('0'));
	QString strMin = QString("%1,%2,%3").arg(minPos[0], 15, 'f', 6, QLatin1Char('0')).arg(minPos[1], 15, 'f', 6, QLatin1Char('0')).arg(minPos[2], 15, 'f', 6, QLatin1Char('0'));
	QString strBBox = "$$DIMENSION/" + strMin + "," + strMax + "\n";
	strHeader += strBBox;

	strHeader += QString("$$LAYERS/%1\n").arg(layers.size());
	strHeader += "$$HEADEREND";

	file->write(strHeader.toLocal8Bit());

	return true;
}

bool writeCLIFileBinary(QString filePath, std::vector<SlicerLayer>&layers, float unit)
{
	QString fileName = filePath;
	QFile cliFile(fileName);
	cliFile.open(QIODevice::WriteOnly);

	writeCLIHeader(&cliFile, layers, unit);
	// Write layers.
	unsigned short int layerLong = 127, polygonLong = 130;
	unsigned int id = 0, dir = 0, pNum = 0;
	float layerZ = -1;

	for (int i = 0; i < layers.size(); i++) {
		const SlicerLayer& layer = layers[i];
		layerZ = INT2MM(layer.z) / unit;
		cliFile.write(reinterpret_cast<char*>(&layerLong), 2);
		cliFile.write(reinterpret_cast<char*>(&layerZ), 4);

		const ClipperLib::Paths& paths = layer.polygons.paths;
		for (int j = 0; j < paths.size(); j++) {
			ClipperLib::Path path = paths[j];

			dir = Orientation(path) == true ? 1 : 0;
			pNum = path.size();

			cliFile.write(reinterpret_cast<char*>(&polygonLong), 2);
			cliFile.write(reinterpret_cast<char*>(&id), 4);
			cliFile.write(reinterpret_cast<char*>(&dir), 4);
			cliFile.write(reinterpret_cast<char*>(&pNum), 4);

			for (int k = 0; k < pNum; k++) {
				float x = INT2MM(path[k].X) / unit;
				float y = INT2MM(path[k].Y) / unit;
				cliFile.write(reinterpret_cast<char*>(&x), 4);
				cliFile.write(reinterpret_cast<char*>(&y), 4);
			}
		}
	}

	cliFile.close();
	return true;
}


int main()
{
	FILE* fp = fopen("slice_time_info.txt", "a");

	auto start = system_clock::now();
	const char* fileName = "D:\\Merge_of_shell_01_of_20240113_shell_64_of_20240113.stl";
	std::cout << "Start Slicing " << fileName << std::endl;
	fprintf(fp, "Start Slicing %s\n", fileName);

	MeshGroup* meshGroup = new MeshGroup();
	FMatrix4x3 matriix;

	loadMeshIntoMeshGroup(meshGroup, fileName, matriix);
	auto readEnd = system_clock::now();
	auto readTime = duration_cast<microseconds>(readEnd - start);
	std::cout << "Read STL time : " << double(readTime.count()) * microseconds::period::num / microseconds::period::den << "취\n";
	fprintf(fp, "Read STL time : %fs\n", double(readTime.count()) * microseconds::period::num / microseconds::period::den);
	
	Mesh* mesh = &meshGroup->meshes.front();
	AABB3D bbox = mesh->getAABB();
	float thickness = 1000;
	int sliceLayerCount = bbox.max.z / thickness;
	std::cout << "Slice Thickness :" << thickness / 1000 << " mm\n";
	fprintf(fp, "Slice Thickness:%f mm\n", thickness / 1000);

	Slicer slicer(mesh, thickness, sliceLayerCount, false);
	auto sliceEnd = system_clock::now();
	auto sliceTime = duration_cast<microseconds>(sliceEnd - readEnd);
	std::cout << "Slice time : " << double(sliceTime.count()) * microseconds::period::num / microseconds::period::den << "취\n";
	fprintf(fp, "Slice time : %fs\n", double(sliceTime.count()) * microseconds::period::num / microseconds::period::den);
	
	QString filePath = "D:\\Merge_of_shell_01_of_20240113_shell_64_of_20240113.cli";
	writeCLIFileBinary(filePath, slicer.layers, 0.01);
	auto writeEnd = system_clock::now();
	auto writeTime = duration_cast<microseconds>(writeEnd - sliceEnd);
	std::cout << "Write time : " << double(writeTime.count()) * microseconds::period::num / microseconds::period::den << "취\n";
	fprintf(fp, "Write time : %fs\n", double(writeTime.count()) * microseconds::period::num / microseconds::period::den);
	
	auto end = system_clock::now();
	auto allTime = duration_cast<microseconds>(end - start);
	std::cout << "All time : " << double(allTime.count()) * microseconds::period::num / microseconds::period::den << "취\n";
	fprintf(fp, "All time : %fs\n\n\n", double(allTime.count()) * microseconds::period::num / microseconds::period::den);
	
	return 0;

}