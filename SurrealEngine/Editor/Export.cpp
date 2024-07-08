#include "miniz/miniz.h"
#include "Editor/Export.h"
#include "Package/Package.h"
#include "UObject/UProperty.h"
#include "UObject/UTextBuffer.h"

static void UnpackRgba(uint32_t rgba, uint32_t& r, uint32_t& g, uint32_t& b, uint32_t& a)
{
	r = rgba & 0xff;
	g = (rgba >> 8) & 0xff;
	b = (rgba >> 16) & 0xff;
	a = (rgba >> 24) & 0xff;
}

/////////////////////////////////////////////////////////////////////////////

std::string Exporter::ExportObject(UObject* obj, int tablevel, bool bInline)
{
	std::string txt = "";
	for (UProperty* prop : obj->Class->Properties)
	{
		for (int i = 0; i < prop->ArrayDimension; i++)
		{
			if (AnyFlags(prop->Flags, ObjectFlags::TagExp))
			{
				// Get default property from super class
				UObject* defobj = nullptr;
				if (obj->Class == obj->Class->Class)
					defobj = obj->Class->BaseStruct;
				else
					defobj = obj->Class;

				if (prop->Name == "Tag")
				{
					if (obj->Class->GetPropertyAsString(prop->Name) == obj->Name.ToString())
						continue;
				}

				// TODO: implement inline declared objects here
				// not necessary until we support <= 227j

				std::string tabs(tablevel, '\t');
				prop->GetExportText(txt, tabs, obj, defobj, i);
			}
		}
	}

	return txt;
}

/////////////////////////////////////////////////////////////////////////////

MemoryStreamWriter Exporter::ExportClass(UClass* cls)
{
	MemoryStreamWriter text;
	if (!cls->ScriptText)
		return text;

	text << cls->ScriptText->Text;
	text << "\r\ndefaultproperties\r\n{\r\n";
	text << ExportObject(cls->GetDefaultObject<UObject>(), 1, false);
	return text;
}

/////////////////////////////////////////////////////////////////////////////

MemoryStreamWriter Exporter::ExportFont(UFont* font)
{
	MemoryStreamWriter text;

	const Array<FontPage>& pages = font->GetPages();
	text << "BEGIN OBJECT CLASS=Font\r\n";

	for (const FontPage& page : pages)
	{
		text << "\tBEGIN PAGE\r\n";
		text << "\t\tTexture='" + page.Texture->Name.ToString() + "'\r\n";

		for (int i = 0; i < page.Characters.size(); i++)
		{
			const FontCharacter& c = page.Characters[i];
			int StartU = c.StartU;
			int StartV = c.StartV;
			int USize = c.VSize;
			int VSize = c.USize;

			if (!StartU && !StartV && !USize && !VSize)
				continue;

			text << "\t\tChar" + std::to_string(i) +  "(StartU=" + std::to_string(StartU) + ",";
			text << "StartV=" + std::to_string(StartV) + ",";
			text << "USize=" + std::to_string(USize) + ",";
			text << "VSize=" + std::to_string(VSize) + ")\r\n";
		}

		text << "\tEND PAGE\r\n";
	}

	text << "END OBJECT\r\n";
	return text;
}

/////////////////////////////////////////////////////////////////////////////

// "James Mesh Types"
// https://paulbourke.net/dataformats/unreal/
enum JmtFlags
{
	Normal = 0,
	TwoSided = 1,
	Translucent = 2,
	MaskedTwoSided = 3,
	ModulatedTwoSided = 4,
	WeaponTriangle = 8
};

struct U3DAnivHeader
{
	uint16_t NumFrames;
	uint16_t FrameSize;
};

struct U3DDataHeader
{
	uint16_t NumPolygons;
	uint16_t NumVertices;
	uint16_t BogusRot;
	uint16_t BogusFrame;
	uint32_t BogusNormX;
	uint32_t BogusNormY;
	uint32_t BogusNormZ;
	uint32_t FixScale;
	uint32_t Unused[3];
	uint8_t Unknown[12];
};

struct U3DDataTriangle
{
	uint16_t Vertex[3];
	int8_t Type;
	int8_t Color;
	uint8_t VertexUV[3][2];
	int8_t TexNum;
	int8_t Flags; // unused
};

MemoryStreamWriter& operator<<(MemoryStreamWriter& s, U3DAnivHeader& hdr)
{
	s << hdr.NumFrames;
	s << hdr.FrameSize;
	return s;
}

MemoryStreamWriter& operator<<(MemoryStreamWriter& s, U3DDataHeader& hdr)
{
	s << hdr.NumPolygons;
	s << hdr.NumVertices;
	s << hdr.BogusRot;
	s << hdr.BogusFrame;
	s << hdr.BogusNormX;
	s << hdr.BogusNormY;
	s << hdr.BogusNormZ;
	s << hdr.FixScale;

	for (int i = 0; i < sizeof(hdr.Unused) / sizeof(hdr.Unused[0]); i++)
	{
		s << hdr.Unused[i];
	}

	for (int i = 0; i < sizeof(hdr.Unknown) / sizeof(hdr.Unknown[0]); i++)
	{
		s << hdr.Unknown[i];
	}

	return s;
}

MemoryStreamWriter& operator<<(MemoryStreamWriter& s, U3DDataTriangle& tri)
{
	for (int i = 0; i < sizeof(tri.Vertex) / sizeof(tri.Vertex[0]); i++)
	{
		s << tri.Vertex[i];
	}

	s << tri.Type;
	s << tri.Color;
	s.Write(tri.VertexUV, sizeof(tri.VertexUV));
	s << tri.TexNum;
	s << tri.Flags;

	return s;
}

/////////////////////////////////////////////////////////////////////////////

MemoryStreamWriter Exporter::ExportMeshAnim(UMesh* mesh)
{
	MemoryStreamWriter data;

	if (!mesh)
	{
		return data;
	}

	const std::string& className = mesh->Class->Name.ToString();
	if (className.compare("SkeletalMesh") == 0)
	{
		return data;
	}

	U3DAnivHeader hdr;
	hdr.NumFrames = mesh->AnimFrames;
	hdr.FrameSize = mesh->FrameVerts * 4;
	data << hdr;

	uint32_t uvtx;
	for (int i = 0; i < mesh->AnimFrames; i++)
	{
		for (int k = 0; k < mesh->FrameVerts; k++)
		{
			size_t index = (size_t)((i * mesh->FrameVerts) + k);
			vec3& vtx = mesh->Verts[index];
			uvtx = ((int)(-vtx.x * 8.0) & 0x7ff) | (((int)(-vtx.y * 8.0) & 0x7ff) << 11) | (((int)(vtx.z * 4.0) & 0x3ff) << 22);
			data << uvtx;
		}
	}

	return data;
}

MemoryStreamWriter Exporter::ExportMeshData(UMesh* mesh)
{
	MemoryStreamWriter data;
	if (!mesh)
	{
		return data;
	}

	const std::string& className = mesh->Class->Name.ToString();
	if (className.compare("LodMesh") == 0)
	{
		return ExportLodMesh(reinterpret_cast<ULodMesh*>(mesh));
	}
	if (className.compare("SkeletalMesh") == 0)
	{
		return ExportSkeletalMesh(reinterpret_cast<USkeletalMesh*>(mesh));
	}

	U3DDataHeader hdr;
	size_t numPolygons = mesh->Tris.size();
	if (numPolygons > UINT16_MAX)
	{
		Exception::Throw("Too many triangles (" + std::to_string(numPolygons) + ") to export in " + mesh->Name.ToString());
	}

	hdr.NumPolygons = static_cast<uint16_t>(numPolygons);
	hdr.NumVertices = mesh->FrameVerts;
	data << hdr;

	U3DDataTriangle tri;
	for (MeshTri& mt : mesh->Tris)
	{
		for (int i = 0; i < 3; i++)
		{
			tri.Vertex[i] = mt.Indices[i];
			tri.VertexUV[i][0] = mt.UV[i].s;
			tri.VertexUV[i][1] = mt.UV[i].t;
		}

		if (mt.PolyFlags & PF_TwoSided)
		{
			if (mt.PolyFlags & PF_Modulated)
				tri.Type = JmtFlags::ModulatedTwoSided;
			else if (mt.PolyFlags & PF_Masked)
				tri.Type = JmtFlags::MaskedTwoSided;
			else if (mt.PolyFlags & PF_Translucent)
				tri.Type = JmtFlags::Translucent;
			else
				tri.Type = JmtFlags::TwoSided;
		}
		else
		{
			tri.Type = JmtFlags::Normal;
		}

		tri.Color = 127; // TODO: not sure what this should be
		tri.TexNum = mt.TextureIndex;
		tri.Flags = 0;

		data << tri;
	}

	return data;
}

MemoryStreamWriter Exporter::ExportLodMesh(ULodMesh* mesh)
{
	MemoryStreamWriter data;

	U3DDataHeader hdr;
	size_t numPolygons = mesh->Faces.size();
	if (numPolygons > UINT16_MAX)
	{
		Exception::Throw("Too many triangles (" + std::to_string(numPolygons) + ") to export in " + mesh->Name.ToString());
	}

	hdr.NumPolygons = static_cast<uint16_t>(numPolygons);
	hdr.NumVertices = mesh->FrameVerts;
	data << hdr;

	U3DDataTriangle tri;
	for (int i = 0; i < numPolygons; i++)
	{
		MeshFace& face = mesh->Faces[i];
		for (int k = 0; k < 3; k++)
		{
			MeshWedge& wedge = mesh->Wedges[face.Indices[k]];
			tri.Vertex[k] = wedge.Vertex;
			tri.VertexUV[k][0] = wedge.U;
			tri.VertexUV[k][1] = wedge.V;
		}

		MeshMaterial& mat = mesh->Materials[face.MaterialIndex];
		if (mesh->SpecialFaces.size() == 1 && i == mesh->Faces.size() - 1)
		{
			tri.Type = JmtFlags::WeaponTriangle;
		}
		else if (mat.PolyFlags & PF_TwoSided)
		{
			if (mat.PolyFlags & PF_Modulated)
				tri.Type = JmtFlags::ModulatedTwoSided;
			else if (mat.PolyFlags & PF_Masked)
				tri.Type = JmtFlags::MaskedTwoSided;
			else if (mat.PolyFlags & PF_Translucent)
				tri.Type = JmtFlags::Translucent;
			else
				tri.Type = JmtFlags::TwoSided;
		}
		else
		{
			tri.Type = JmtFlags::Normal;
		}

		tri.Color = 127; // TODO: not sure what this should be
		tri.TexNum = mat.TextureIndex;
		tri.Flags = 0;

		data << tri;
	}

	return data;
}

/////////////////////////////////////////////////////////////////////////////

MemoryStreamWriter Exporter::ExportMusic(UMusic* music)
{
	if (!music)
		return MemoryStreamWriter();

	return MemoryStreamWriter(music->Data);
}

/////////////////////////////////////////////////////////////////////////////

MemoryStreamWriter Exporter::ExportSound(USound* sound)
{
	MemoryStreamWriter data;

	if (!sound)
		return MemoryStreamWriter();

	return MemoryStreamWriter(sound->Data);
}

/////////////////////////////////////////////////////////////////////////////

MemoryStreamWriter Exporter::ExportSkeletalAnim(UAnimation* anim)
{
	return MemoryStreamWriter();
}

MemoryStreamWriter Exporter::ExportSkeletalMesh(USkeletalMesh* mesh)
{
	return MemoryStreamWriter();
}

/////////////////////////////////////////////////////////////////////////////

MemoryStreamWriter Exporter::ExportTexture(UTexture* tex, const std::string& ext)
{
	if (!tex)
		return MemoryStreamWriter();

	const std::string& className = tex->Class->Name.ToString();
	if (className.compare("FireTexture") == 0)
	{
		return ExportFireTexture(reinterpret_cast<UFireTexture*>(tex));
	}
	else if (className.compare("WetTexture") == 0)
	{
		return ExportWetTexture(reinterpret_cast<UWetTexture*>(tex));
	}
	else if (className.compare("WaveTexture") == 0)
	{
		return ExportWaveTexture(reinterpret_cast<UWaveTexture*>(tex));
	}
	else if (className.compare("IceTexture") == 0)
	{
		return ExportIceTexture(reinterpret_cast<UIceTexture*>(tex));
	}

	if (tex->ActualFormat == TextureFormat::P8)
	{
		if (ext.compare("bmp") == 0)
			return ExportBmpIndexed(tex);
	}

	if (ext.compare("png") == 0)
		return ExportPng(tex);

	Exception::Throw("Unknown texture export format: " + ext);
}

/////////////////////////////////////////////////////////////////////////////

MemoryStreamWriter Exporter::ExportFireTexture(UFireTexture* tex)
{
	MemoryStreamWriter data;
	data << "BEGIN OBJECT CLASS=FireTexture USIZE=" << std::to_string(tex->USize()) << " VSIZE=" << std::to_string(tex->VSize()) << "\r\n";

	data << ExportObject(tex, 1, false);

	// 5-color interpolated palette, this is what 227/469 UnrealEd recognizes
	UPalette* palette = tex->Palette();
	for (int i = 0; i < 5; i++)
	{
		uint32_t r, g, b, a;
		UnpackRgba(palette->Colors[(i * 64) - 1], r, g, b, a);

		data << "\tColor" << std::to_string(i + 1);
		data << "=(R=" << std::to_string(r);
		data << ",G=" << std::to_string(g);
		data << ",B=" << std::to_string(b);
		data << ",A=" << std::to_string(a);
		data << ")\r\n";
	}

	// Export full color palette as well
	for (int i = 0; i < 256; i++)
	{
		uint32_t r, g, b, a;
		UnpackRgba(palette->Colors[i], r, g, b, a);

		data << "\tPaletteColor" << std::to_string(i + 1);
		data << "=(R=" << std::to_string(r);
		data << ",G=" << std::to_string(g);
		data << ",B=" << std::to_string(b);
		data << ",A=" << std::to_string(a);
		data << ")\r\n";
	}

	data << "END OBJECT\r\n";
	return data;
}

MemoryStreamWriter Exporter::ExportWaveTexture(UWaveTexture* tex)
{
	MemoryStreamWriter data;
	data << "BEGIN OBJECT CLASS=WaveTexture USIZE=" + std::to_string(tex->USize()) + " VSIZE=" + std::to_string(tex->VSize()) + "\r\n";

	data << ExportObject(tex, 1, false);

	// 5-color interpolated palette, this is what 227/469 UnrealEd recognizes
	UPalette* palette = tex->Palette();
	for (int i = 0; i < 5; i++)
	{
		uint32_t r, g, b, a;
		UnpackRgba(palette->Colors[(i * 64) - 1], r, g, b, a);

		data << "\tColor" << std::to_string(i + 1);
		data << "=(R=" << std::to_string(r);
		data << ",G=" << std::to_string(g);
		data << ",B=" << std::to_string(b);
		data << ",A=" << std::to_string(a);
		data << ")\r\n";
	}

	// Export full color palette as well
	for (int i = 0; i < 256; i++)
	{
		uint32_t r, g, b, a;
		UnpackRgba(palette->Colors[i], r, g, b, a);

		data << "\tPalette" << std::to_string(i + 1);
		data << "=(R=" << std::to_string(r);
		data << ",G=" << std::to_string(g);
		data << ",B=" << std::to_string(b);
		data << ",A=" << std::to_string(a);
		data << ")\r\n";
	}

	data << "END OBJECT\r\n";
	return data;
}

MemoryStreamWriter Exporter::ExportWetTexture(UWetTexture* tex)
{
	MemoryStreamWriter data;
	data << "BEGIN OBJECT CLASS=WetTexture USIZE=" + std::to_string(tex->USize()) + " VSIZE=" + std::to_string(tex->VSize()) + "\r\n";

	data << ExportObject(tex, 1, false);

	// 5-color interpolated palette, this is what 227/469 UnrealEd recognizes
	UPalette* palette = tex->Palette();
	for (int i = 0; i < 5; i++)
	{
		uint32_t r, g, b, a;
		UnpackRgba(palette->Colors[(i * 64) - 1], r, g, b, a);

		data << "\tColor" << std::to_string(i + 1);
		data << "=(R=" << std::to_string(r);
		data << ",G=" << std::to_string(g);
		data << ",B=" << std::to_string(b);
		data << ",A=" << std::to_string(a);
		data << ")\r\n";
	}

	// Export full color palette as well
	for (int i = 0; i < 256; i++)
	{
		uint32_t r, g, b, a;
		UnpackRgba(palette->Colors[i], r, g, b, a);

		data << "\tPalette" << std::to_string(i + 1);
		data << "=(R=" << std::to_string(r);
		data << ",G=" << std::to_string(g);
		data << ",B=" << std::to_string(b);
		data << ",A=" << std::to_string(a);
		data << ")\r\n";
	}

	data << "END OBJECT\r\n";
	return data;
}

MemoryStreamWriter Exporter::ExportIceTexture(UIceTexture* tex)
{
	MemoryStreamWriter data;
	data << "BEGIN OBJECT CLASS=IceTexture USIZE=" << std::to_string(tex->USize()) << " VSIZE=" << std::to_string(tex->VSize()) << "\r\n";

	data << ExportObject(tex, 1, false);

	// 5-color interpolated palette, this is what 227/469 UnrealEd recognizes
	UPalette* palette = tex->Palette();
	for (int i = 0; i < 5; i++)
	{
		uint32_t r, g, b, a;
		UnpackRgba(palette->Colors[(i * 64) - 1], r, g, b, a);

		data << "\tColor" << std::to_string(i + 1);
		data << "=(R=" << std::to_string(r);
		data << ",G=" << std::to_string(g);
		data << ",B=" << std::to_string(b);
		data << ",A=" << std::to_string(a);
		data << ")\r\n";
	}

	// Export full color palette as well
	for (int i = 0; i < 256; i++)
	{
		uint32_t r, g, b, a;
		UnpackRgba(palette->Colors[i], r, g, b, a);

		data << "\tPalette" << std::to_string(i + 1);
		data << "=(R=" << std::to_string(r);
		data << ",G=" << std::to_string(g);
		data << ",B=" << std::to_string(b);
		data << ",A=" << std::to_string(a);
		data << ")\r\n";
	}

	data << "END OBJECT\r\n";
	return data;
}

/////////////////////////////////////////////////////////////////////////////

struct BmpHeader
{
	uint16_t signature;
	uint32_t fileSize;
	uint32_t reserved;
	uint32_t pixelOffset;

	uint32_t dibHeaderSize;
	uint32_t imageWidth;
	uint32_t imageHeight;
	uint16_t planes;
	uint16_t bitCount;
	uint32_t compression;
	uint32_t imageSize;
	uint32_t pixelsPerMeterX;
	uint32_t pixelsPerMeterY;
	uint32_t colorsUsed;
	uint32_t colorsImportant;
};

MemoryStreamWriter& operator<<(MemoryStreamWriter& s, BmpHeader& bmp)
{
	s << bmp.signature;
	s << bmp.fileSize;
	s << bmp.reserved;
	s << bmp.pixelOffset;

	s << bmp.dibHeaderSize;
	s << bmp.imageWidth;
	s << bmp.imageHeight;
	s << bmp.planes;
	s << bmp.bitCount;
	s << bmp.compression;
	s << bmp.imageSize;
	s << bmp.pixelsPerMeterX;
	s << bmp.pixelsPerMeterY;
	s << bmp.colorsUsed;
	s << bmp.colorsImportant;
	return s;
}

MemoryStreamWriter Exporter::ExportBmpIndexed(UTexture* tex)
{
	MemoryStreamWriter data;
	BmpHeader hdr = { 0 };
	int usize = tex->USize();
	int vsize = tex->VSize();

	if (usize > 8192 || vsize > 8192)
	{
		Exception::Throw("Abnormally large indexed texture: " + std::to_string(usize) + "x" + std::to_string(vsize));
	}

	data.Reserve(sizeof(hdr) + (4 * 256) + (usize * vsize));

	hdr.signature = 0x4d42;
	hdr.fileSize = 0; // re-fill later
	hdr.pixelOffset = sizeof(BmpHeader) + (4 * 256);
	hdr.dibHeaderSize = 40;
	hdr.imageWidth = tex->USize();
	hdr.imageHeight = tex->VSize();
	hdr.planes = 1;
	hdr.bitCount = 8;

	data << hdr;

	UPalette* palette = tex->Palette();

	// Assuming 8-bit color palette
	for (int i = 0; i < 256; i++)
	{
		uint32_t r, g, b, a;
		UnpackRgba(palette->Colors[i], r, g, b, a);

		uint32_t bgra = (b) | (g << 8) | (r << 16) | (a << 24);
		data << bgra;
	}

	hdr.pixelOffset = (uint32_t)data.Tell();

	uint8_t *pixels = tex->Mipmaps[0].Data.data();
	for (int y = vsize; y > 0; y--)
	{
		for (int x = 0; x < usize; x++)
		{
			data << pixels[((y - 1) * usize) + x];
		}
	}

	hdr.fileSize = static_cast<uint32_t>(data.Tell());
	data.Seek(0, SEEK_SET);
	data << hdr;

	return data;
}

MemoryStreamWriter Exporter::ExportPng(UTexture* tex)
{
	MemoryStreamWriter image = GetImage(tex);
	MemoryStreamWriter data;

	size_t pLen = 0;
	void* png = tdefl_write_image_to_png_file_in_memory_ex(image.Data(), tex->USize(), tex->VSize(), 4, &pLen, MZ_BEST_COMPRESSION, 0);

	data.Write(png, pLen);
	return data;
}

MemoryStreamWriter Exporter::GetImage(UTexture* tex)
{
	switch (tex->ActualFormat)
	{
	case TextureFormat::P8:
		return GetImageP8(tex);
	default:
		Exception::Throw(tex->Name.ToString() + "Unimplemented for texture type" + std::to_string(static_cast<int>(tex->ActualFormat)));
	}
}

MemoryStreamWriter Exporter::GetImageP8(UTexture* tex)
{
	MemoryStreamWriter data;
	UPalette* palette = tex->Palette();
	uint8_t* pixels = tex->Mipmaps[0].Data.data();

	int usize = tex->USize();
	int vsize = tex->VSize();

	for (int y = 0; y < vsize; y++)
	{
		for (int x = 0; x < usize; x++)
		{
			int color = pixels[(y * usize) + x];
			data << palette->Colors[color];
		}
	}

	return data;
}
