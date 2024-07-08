#pragma once 

#include "Utils/MemoryStreamWriter.h"
#include "UObject/UClass.h"
#include "UObject/UFont.h"
#include "UObject/ULevel.h"
#include "UObject/UMesh.h"
#include "UObject/UMusic.h"
#include "UObject/USound.h"
#include "UObject/UTexture.h"

class Exporter
{
public:
	static std::string ExportObject(UObject* obj, int tablevel, bool bInline);

	static MemoryStreamWriter ExportClass(UClass* cls);
	static MemoryStreamWriter ExportFont(UFont* font);
	static MemoryStreamWriter ExportLevel(ULevel* level);
	static MemoryStreamWriter ExportMeshAnim(UMesh* mesh);
	static MemoryStreamWriter ExportMeshData(UMesh* mesh);
	static MemoryStreamWriter ExportMusic(UMusic* music);
	static MemoryStreamWriter ExportSkeletalAnim(UAnimation* anim);
	static MemoryStreamWriter ExportSkeletalMesh(USkeletalMesh* mesh);
	static MemoryStreamWriter ExportSound(USound* sound);
	static MemoryStreamWriter ExportTexture(UTexture* tex, const std::string& ext);

private:
	static MemoryStreamWriter ExportFireTexture(UFireTexture* tex);
	static MemoryStreamWriter ExportWaveTexture(UWaveTexture* tex);
	static MemoryStreamWriter ExportWetTexture(UWetTexture* tex);
	static MemoryStreamWriter ExportIceTexture(UIceTexture* tex);

	static MemoryStreamWriter ExportBmpIndexed(UTexture* tex);
	static MemoryStreamWriter ExportPng(UTexture* tex);

	static MemoryStreamWriter GetImage(UTexture* tex);
	static MemoryStreamWriter GetImageP8(UTexture* tex);

	static MemoryStreamWriter ExportLodMesh(ULodMesh* mesh);
};