
#include "Precomp.h"
#include "ExportCommandlet.h"
#include "DebuggerApp.h"
#include "Engine.h"
#include "File.h"
#include "Package/PackageManager.h"
#include "Package/Package.h"
#include "Editor/Export.h"
#include "UObject/UClass.h"
#include "UObject/UTextBuffer.h"

typedef std::pair<Package*, std::string> PackageNamePair;

enum class ExportCommand
{
	All,
	Scripts,
	Textures,
	Fonts,
	Sounds,
	Music,
	Meshes,
	Level,
	Unknown
};

ExportCommandlet::ExportCommandlet()
{
	SetLongFormName("export");
	SetShortDescription("Extract data from the packages");
}

static ExportCommand GetCommand(const std::string& command)
{
	static std::pair<std::string, ExportCommand> commands[] =
	{
		{"all", ExportCommand::All},
		{"scripts", ExportCommand::Scripts},
		{"textures", ExportCommand::Textures},
		{"fonts", ExportCommand::Fonts},
		{"sounds", ExportCommand::Sounds},
		{"music", ExportCommand::Music},
		{"meshes", ExportCommand::Meshes},
		{"level", ExportCommand::Level}
	};

	std::string lowerCmd = command;
	std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), [](unsigned char c) { return tolower(c); });

	for (int i = 0; i < (sizeof(commands) / sizeof(std::pair<std::string, ExportCommand>)); i++)
	{
		auto& iter = commands[i];
		if (lowerCmd.compare(iter.first) == 0)
			return iter.second;
	}

	return ExportCommand::Unknown;
}

void ExportCommandlet::OnCommand(DebuggerApp* console, const std::string& args)
{
	if (console->launchinfo.gameRootFolder.empty())
	{
		console->WriteOutput("Root Folder section of LaunchInfo is empty!" + NewLine());
		return;
	}

	packageNames.clear();

	std::string argsStripped = args.substr(0, args.find_last_not_of(' ') + 1);

	size_t argsSep = argsStripped.find_first_of(' ');
	std::string cmdString = argsStripped.substr(0, argsSep);
	std::string cmdArgs = argsStripped.substr(argsSep+1);

	ExportCommand cmd = GetCommand(cmdString);
	if (cmd == ExportCommand::Unknown)
	{
		console->WriteOutput("Unknown command " + args + NewLine());
		return;
	}

	std::vector<std::string> packages;
	if (argsStripped.size() != cmdArgs.size())
	{
		size_t sep = cmdArgs.find_first_of(' ');
		do
		{
			cmdArgs = cmdArgs.substr(0, sep);
			packages.push_back(cmdArgs);
		} while (sep != std::string::npos);
	}

	switch (cmd)
	{
	case ExportCommand::All:
		ExportAll(console, packages);
		break;
	case ExportCommand::Scripts:
		ExportScripts(console, packages);
		break;
	case ExportCommand::Textures:
		ExportTextures(console, packages);
		break;
	case ExportCommand::Fonts:
		ExportFonts(console, packages);
		break;
	case ExportCommand::Sounds:
		ExportSounds(console, packages);
		break;
	case ExportCommand::Music:
		ExportMusic(console, packages);
		break;
	case ExportCommand::Meshes:
		ExportMeshes(console, packages);
		break;
	case ExportCommand::Level:
		ExportLevel(console, packages);
		break;
	}

	console->WriteOutput("Done." + NewLine() + NewLine());
}

/////////////////////////////////////////////////////////////////////////////

void ExportCommandlet::ExportAll(DebuggerApp* console, std::vector<std::string>& packages)
{
	console->WriteOutput("Unimplemented" + NewLine());
}

/////////////////////////////////////////////////////////////////////////////

void ExportCommandlet::ExportScripts(DebuggerApp* console, std::vector<std::string>& packages)
{
	InitExport(packages);

	if (packages.size() == 0)
		console->WriteOutput("Checking all packages..." + NewLine());

	// cull out packages without scripts
	std::vector<PackageNamePair> packageObjects;
	for (std::string pkgname : packageNames)
	{
		if (pkgname == "Editor")
			continue;

		Package* package = engine->packages->GetPackage(pkgname);
		if (package->HasObjectOfType<UClass>())
			packageObjects.push_back(PackageNamePair(package, pkgname));
	}
	
	if (packageObjects.size() == 0)
	{
		console->WriteOutput("No scripts found");
		return;
	}

	for (PackageNamePair& pkgobject : packageObjects)
	{
		Package* package = pkgobject.first;
		std::string& name = pkgobject.second;

		std::string pkgname = package->GetPackageName().ToString();
		std::string pkgpath = FilePath::combine(engine->LaunchInfo.gameRootFolder, name);
		std::string path = FilePath::combine(pkgpath, "Classes");
		bool pkgpathcreated = false;

		console->WriteOutput("Exporting scripts from " + ColorEscape(96) + name + ResetEscape() + NewLine());

		std::vector<UClass*> classes = package->GetAllObjects<UClass>();

		for (UClass* cls : classes)
		{
			MemoryStreamWriter stream = Exporter::ExportClass(cls);
			if (stream.Size() > 0)
			{
				if (!pkgpathcreated)
				{
					Directory::make_directory(pkgpath);
					Directory::make_directory(path);
					pkgpathcreated = true;
				}
				std::string filename = FilePath::combine(path, cls->FriendlyName.ToString() + ".uc");
				File::write_all_bytes(filename, stream.Data(), stream.Size());
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

const std::vector<std::string> formats =
{
	"bmp",
	"png"
};

void ExportCommandlet::ExportTextures(DebuggerApp* console, std::vector<std::string>& packages)
{
	InitExport(packages);

	if (packages.size() == 0)
		console->WriteOutput("Checking all packages..." + NewLine());

	// cull out packages without textures
	std::vector<PackageNamePair> packageObjects;
	for (std::string pkgname : packageNames)
	{
		if (pkgname == "Editor")
			continue;

		Package* package = engine->packages->GetPackage(pkgname);
		if (package->HasObjectOfType<UTexture>())
			packageObjects.push_back(PackageNamePair(package, pkgname));
	}

	if (packageObjects.size() == 0)
	{
		console->WriteOutput("No textures found" + NewLine());
		return;
	}
	
	// TODO: ini setting which specifies choice automatically?
	// Ask for texture format
	console->WriteOutput("Input desired texture format:" + console->NewLine());
	for (const std::string& format : formats)
	{
		console->WriteOutput("\t" + format + console->NewLine());
	}
	std::string desiredExt = console->GetInput();
	std::transform(desiredExt.begin(), desiredExt.end(), desiredExt.begin(), [](unsigned char c) { return tolower(c); });

	bool validExt = false;
	for (const std::string& format : formats)
	{
		if (format.compare(desiredExt))
		{
			validExt = true;
			break;
		}
	}

	if (!validExt)
	{
		console->WriteOutput("Unknown format " + desiredExt + console->NewLine());
		return;
	}

	for (PackageNamePair& pkgobject : packageObjects)
	{
		Package* package = pkgobject.first;
		std::string& name = pkgobject.second;

		std::string pkgname = package->GetPackageName().ToString();
		std::string pkgpath = FilePath::combine(engine->LaunchInfo.gameRootFolder, name);
		std::string texturespath = FilePath::combine(pkgpath, "Textures");
		bool pkgpathcreated = false;

		console->WriteOutput("Exporting textures from " + ColorEscape(96) + name + ResetEscape() + NewLine());

		std::vector<UTexture*> textures = package->GetAllObjects<UTexture>();

		for (UTexture* tex : textures)
		{
			// TODO: support more formats than just bmp
			std::string ext;
			if (tex->IsA("FractalTexture"))
				ext.assign("fx");
			else
				ext.assign(desiredExt);

			MemoryStreamWriter stream = Exporter::ExportTexture(tex, ext);
			if (stream.Size() > 0)
			{
				if (!pkgpathcreated)
				{
					Directory::make_directory(pkgpath);
					Directory::make_directory(texturespath);
					pkgpathcreated = true;
				}

				std::string filename = FilePath::combine(texturespath, tex->Name.ToString() + "." + ext);
				File::write_all_bytes(filename, stream.Data(), stream.Size());
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void ExportCommandlet::ExportFonts(DebuggerApp* console, std::vector<std::string>& packages)
{
	InitExport(packages);

	if (packages.size() == 0)
		console->WriteOutput("Checking all packages..." + NewLine());

	// cull out packages without fonts
	std::vector<PackageNamePair> packageObjects;
	for (std::string pkgname : packageNames)
	{
		if (pkgname == "Editor")
			continue;

		Package* package = engine->packages->GetPackage(pkgname);
		if (package->HasObjectOfType<UFont>())
			packageObjects.push_back(PackageNamePair(package, pkgname));
	}

	if (packageObjects.size() == 0)
	{
		console->WriteOutput("No fonts found" + NewLine());
		return;
	}

	// TODO: ini setting which specifies choice automatically?
	// Ask for texture format
	console->WriteOutput("Input desired texture format:" + console->NewLine());
	for (const std::string& format : formats)
	{
		console->WriteOutput("\t" + format + console->NewLine());
	}
	std::string desiredExt = console->GetInput();
	std::transform(desiredExt.begin(), desiredExt.end(), desiredExt.begin(), [](unsigned char c) { return tolower(c); });

	bool validExt = false;
	for (const std::string& format : formats)
	{
		if (format.compare(desiredExt))
		{
			validExt = true;
			break;
		}
	}

	if (!validExt)
	{
		console->WriteOutput("Unknown format " + desiredExt + console->NewLine());
		return;
	}

	for (PackageNamePair& pkgobject : packageObjects)
	{
		Package* package = pkgobject.first;
		std::string& name = pkgobject.second;

		std::string pkgname = package->GetPackageName().ToString();
		std::string pkgpath = FilePath::combine(engine->LaunchInfo.gameRootFolder, name);
		std::string path = FilePath::combine(pkgpath, "Fonts");
		bool pkgpathcreated = false;

		console->WriteOutput("Exporting fonts from " + ColorEscape(96) + name + ResetEscape() + NewLine());

		std::vector<UFont*> fonts = package->GetAllObjects<UFont>();

		for (UFont* font : fonts)
		{
			MemoryStreamWriter stream = Exporter::ExportFont(font);
			if (stream.Size() > 0)
			{
				if (!pkgpathcreated)
				{
					Directory::make_directory(pkgpath);
					Directory::make_directory(path);
					pkgpathcreated = true;
				}

				std::string filename = FilePath::combine(path, font->Name.ToString() + ".ufnt");
				File::write_all_bytes(filename, stream.Data(), stream.Size());

				const std::vector<FontPage>& pages = font->GetPages();
				for (const FontPage& page : pages)
				{
					MemoryStreamWriter texstream = Exporter::ExportTexture(page.Texture, desiredExt);
					filename = FilePath::combine(path, page.Texture->Name.ToString() + "." + desiredExt);
					File::write_all_bytes(filename, texstream.Data(), texstream.Size());
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void ExportCommandlet::ExportSounds(DebuggerApp* console, std::vector<std::string>& packages)
{
	InitExport(packages);

	if (packages.size() == 0)
		console->WriteOutput("Checking all packages..." + NewLine());

	// cull out packages without sounds
	std::vector<PackageNamePair> packageObjects;
	for (std::string pkgname : packageNames)
	{
		if (pkgname == "Editor")
			continue;

		Package* package = engine->packages->GetPackage(pkgname);
		if (package->HasObjectOfType<USound>())
			packageObjects.push_back(PackageNamePair(package, pkgname));
	}

	if (packageObjects.size() == 0)
	{
		console->WriteOutput("No sounds found" + NewLine());
		return;
	}

	for (PackageNamePair& pkgobject : packageObjects)
	{
		Package* package = pkgobject.first;
		std::string& name = pkgobject.second;

		std::string pkgname = package->GetPackageName().ToString();
		std::string pkgpath = FilePath::combine(engine->LaunchInfo.gameRootFolder, name);
		std::string path = FilePath::combine(pkgpath, "Sounds");
		bool pkgpathcreated = false;

		console->WriteOutput("Exporting sounds from " + ColorEscape(96) + name + ResetEscape() + NewLine());

		std::vector<USound*> sounds = package->GetAllObjects<USound>();

		for (USound* sound : sounds)
		{
			MemoryStreamWriter stream = Exporter::ExportSound(sound);
			if (stream.Size() > 0)
			{
				if (!pkgpathcreated)
				{
					Directory::make_directory(pkgpath);
					Directory::make_directory(path);
					pkgpathcreated = true;
				}

				std::string filename = FilePath::combine(path, sound->Name.ToString() + "." + sound->Format.ToString());
				File::write_all_bytes(filename, stream.Data(), stream.Size());
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void ExportCommandlet::ExportMusic(DebuggerApp* console, std::vector<std::string>& packages)
{
	InitExport(packages);

	if (packages.size() == 0)
		console->WriteOutput("Checking all packages..." + NewLine());

	// cull out packages without music
	std::vector<PackageNamePair> packageObjects;
	for (std::string pkgname : packageNames)
	{
		if (pkgname == "Editor")
			continue;

		Package* package = engine->packages->GetPackage(pkgname);
		if (package->HasObjectOfType<UMusic>())
			packageObjects.push_back(PackageNamePair(package, pkgname));
	}

	if (packageObjects.size() == 0)
	{
		console->WriteOutput("No music found" + NewLine());
		return;
	}

	for (PackageNamePair& pkgobject : packageObjects)
	{
		Package* package = pkgobject.first;
		std::string& name = pkgobject.second;

		std::string pkgname = package->GetPackageName().ToString();
		std::string pkgpath = FilePath::combine(engine->LaunchInfo.gameRootFolder, name);
		std::string path = FilePath::combine(pkgpath, "Music");
		bool pkgpathcreated = false;

		console->WriteOutput("Exporting music from " + ColorEscape(96) + name + ResetEscape() + NewLine());

		std::vector<UMusic*> musics = package->GetAllObjects<UMusic>();

		for (UMusic* music : musics)
		{
			MemoryStreamWriter stream = Exporter::ExportMusic(music);
			if (stream.Size() > 0)
			{
				if (!pkgpathcreated)
				{
					Directory::make_directory(pkgpath);
					Directory::make_directory(path);
					pkgpathcreated = true;
				}

				std::string filename = FilePath::combine(path, music->Name.ToString() + "." + music->Format.ToString());
				File::write_all_bytes(filename, stream.Data(), stream.Size());
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void ExportCommandlet::ExportMeshes(DebuggerApp* console, std::vector<std::string>& packages)
{
	InitExport(packages);

	if (packages.size() == 0)
		console->WriteOutput("Checking all packages..." + NewLine());

	// cull out packages without meshes/animations
	std::vector<PackageNamePair> packageObjects;
	for (std::string pkgname : packageNames)
	{
		if (pkgname == "Editor")
			continue;

		Package* package = engine->packages->GetPackage(pkgname);
		if (package->HasObjectOfType<UMesh>())
			packageObjects.push_back(PackageNamePair(package, pkgname));
	}

	if (packageObjects.size() == 0)
	{
		console->WriteOutput("No meshes/animation found" + NewLine());
		return;
	}

	for (PackageNamePair& pkgobject : packageObjects)
	{
		Package* package = pkgobject.first;
		std::string& name = pkgobject.second;

		std::string pkgname = package->GetPackageName().ToString();
		std::string pkgpath = FilePath::combine(engine->LaunchInfo.gameRootFolder, name);
		std::string path = FilePath::combine(pkgpath, "Meshes");
		bool pkgpathcreated = false;

		console->WriteOutput("Exporting meshes/animation from " + ColorEscape(96) + name + ResetEscape() + NewLine());

		std::vector<UMesh*> meshes = package->GetAllObjects<UMesh>();

		for (UMesh* mesh : meshes)
		{
			std::string dataExt = "_d.3d";
			if (mesh->IsA("SkeletalMesh"))
			{
				// Skip animation export for now
				dataExt = "psk";
			}
			else
			{
				// Export vertex animations
				MemoryStreamWriter animstream = Exporter::ExportMeshAnim(mesh);
				if (animstream.Size() > 0)
				{
					if (!pkgpathcreated)
					{
						Directory::make_directory(pkgpath);
						Directory::make_directory(path);
						pkgpathcreated = true;
					}

					std::string filename = FilePath::combine(path, mesh->Name.ToString() + "_a.3d");
					File::write_all_bytes(filename, animstream.Data(), animstream.Size());
				}
			}

			MemoryStreamWriter datastream = Exporter::ExportMeshData(mesh);
			if (datastream.Size() > 0)
			{
				if (!pkgpathcreated)
				{
					Directory::make_directory(pkgpath);
					Directory::make_directory(path);
					pkgpathcreated = true;
				}

				std::string filename = FilePath::combine(path, mesh->Name.ToString() + dataExt);
				File::write_all_bytes(filename, datastream.Data(), datastream.Size());
			}
		}

		std::vector<UAnimation*> skeletalAnims = package->GetAllObjects<UAnimation>();
		if (skeletalAnims.size() > 0)
		{
			for (UAnimation* anim : skeletalAnims)
			{
				// Export skeletal animations
				MemoryStreamWriter stream = Exporter::ExportSkeletalAnim(anim);
				if (stream.Size() > 0)
				{
					if (!pkgpathcreated)
					{
						Directory::make_directory(pkgpath);
						Directory::make_directory(path);
						pkgpathcreated = true;
					}

					std::string filename = FilePath::combine(path, anim->Name.ToString() + ".psa");
					File::write_all_bytes(filename, stream.Data(), stream.Size());
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void ExportCommandlet::ExportLevel(DebuggerApp* console, std::vector<std::string>& packages)
{
	console->WriteOutput("Unimplemented" + NewLine());
}

/////////////////////////////////////////////////////////////////////////////

void ExportCommandlet::OnPrintHelp(DebuggerApp* console)
{
	console->WriteOutput("Syntax: export <command> (packages)" + NewLine());
	console->WriteOutput("Commands: all scripts textures sounds music meshes level" + NewLine());
}

void ExportCommandlet::InitExport(std::vector<std::string>& packages)
{
	if (packages.size() == 0)
	{
		std::vector<NameString> packageNameStrings = engine->packages->GetPackageNames();
		for (NameString pkgname : packageNameStrings)
			packageNames.push_back(pkgname.ToString());
	}
	else
	{
		for (std::string pkgname : packages)
			packageNames.push_back(pkgname);
	}

	// sort package names alphabetically
	std::sort(packageNames.begin(), packageNames.end());
}
