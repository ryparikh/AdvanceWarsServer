#include "MapParser.h"

#include <cctype>
#include <fstream>
#include <string>
#include <utility>

#include "Map.h"
#include "Result.h"
#include "Terrain.h"
#include "TerrainFileId.h"
#include "UnitInfo.h"

//Result TryParseUnits(std::fstream& filestream, std::unique_ptr<Map>& spMapTemp) noexcept {
//	std::string strStream;
//	UnitProperties::Type unitType = UnitProperties::Type::Invalid;
//	int x = -1;
//	int y = -1;
//	for (char input = filestream.get(); !filestream.eof(); input = filestream.get())
//	{
//		bool fComma = input == ',';
//		bool fNewLine = input == '\n';
//		if (fComma)
//		{
//			if (unitType == UnitProperties::Type::Invalid)
//			{
//				int nUnitType = static_cast<int>(UnitProperties::Type::Invalid);
//				try
//				{
//					nUnitType = std::stoi(strStream);
//				}
//				catch (...)
//				{
//					return Result::Failed;
//				}
//
//				if (!(nUnitType > static_cast<int>(UnitProperties::Type::Invalid) && nUnitType < static_cast<int>(UnitProperties::Type::Size)))
//				{
//					return Result::Failed;
//				}
//
//				unitType = static_cast<UnitProperties::Type>(nUnitType);
//			}
//			else if (x == -1)
//			{
//				try
//				{
//					x = std::stoi(strStream);
//				}
//				catch (...)
//				{
//					return Result::Failed;
//				}
//			}
//
//			strStream.clear();
//			continue;
//		}
//		else if (fNewLine)
//		{
//			try
//			{
//				y = std::stoi(strStream);
//			}
//			catch (...)
//			{
//				return Result::Failed;
//			}
//
//			spMapTemp->TryAddUnit(x, y, unitType);
//			strStream.clear();
//			continue;
//		}
//		else if (!std::isdigit(static_cast<unsigned char>(input)))
//		{
//			return Result::Failed;
//		}
//
//		strStream += input;
//	}
//	return Result::Succeeded;
//}

// Read the file a character at a time and buffer in strStream if the char is a digit.
// Non digit characters are unexpected. If we find a comma or newline, we parse the previous text into an integer
// and map it to the correct terrain type.
Result MapParser::TryCreateFromFile(std::filesystem::path filePath, const std::array<Player, 2>& players, std::unique_ptr<Map>& spMap) noexcept
{
	std::fstream filestream(filePath, std::ios::in);
	if (filestream.fail() || filestream.eof())
	{
		return Result::Failed;
	}

	std::unique_ptr<Map> spMapTemp(new Map());
	std::string strStream;
	int x = 0;
	int y = 0;
	for (char input = filestream.get(); !filestream.eof(); input = filestream.get())
	{
		bool fComma = input == ',';
		bool fNewLine = input == '\n';
		if (fComma || fNewLine)
		{
			int nTileId = static_cast<int>(TerrainFileID::Invalid);
			try
			{
				nTileId = std::stoi(strStream);
			}
			catch (...)
			{
				return Result::Failed;
			}

			if (!(nTileId >= static_cast<int>(TerrainFileID::Plain) && nTileId <= static_cast<int>(TerrainFileID::YellowCometHeadquarters)) &&
				!(nTileId >= static_cast<int>(TerrainFileID::RedFireCity) && nTileId <= static_cast<int>(TerrainFileID::WhiteNovaPort)))
			{
				return Result::Failed;
			}

			if (spMapTemp->TryAddTile(x, y, ToTerrainType(static_cast<TerrainFileID>(nTileId))) == Result::Failed)
			{
				return Result::Failed;
			}

			strStream.clear();
			if (fComma)
			{
				++x;
			}
			else
			{
				x = 0;
				++y;
			}

			continue;
		}
		//else if (input == 'U')
		//{
		//	do
		//	{
		//		input = filestream.get();
		//	} while (input != '\n');

		//	TryParseUnits(filestream, spMapTemp);
		//}
		else if (!std::isdigit(static_cast<unsigned char>(input)))
		{
			return Result::Failed;
		}

		strStream += input;
	}



	spMap.reset(spMapTemp.release());
	return Result::Succeeded;
}

/*static*/ Terrain::Type MapParser::ToTerrainType(TerrainFileID terrainID) noexcept
{
	switch (terrainID)
	{
	default:
	{
		*((int*)0) = 0;
		return Terrain::Type::Plain;
	}
	case TerrainFileID::HPipeRubble:
	case TerrainFileID::VPipeRubble:
	case TerrainFileID::Plain:
	{
		return Terrain::Type::Plain;
	}
	case TerrainFileID::Mountain:
	{
		return Terrain::Type::Mountain;
	}
	case TerrainFileID::Forest:
	{
		return Terrain::Type::Forest;
	}
	case TerrainFileID::HRiver:
	case TerrainFileID::VRiver:
	case TerrainFileID::CRiver:
	case TerrainFileID::ESRiver:
	case TerrainFileID::SWRiver:
	case TerrainFileID::WNRiver:
	case TerrainFileID::NERiver:
	case TerrainFileID::ESWRiver:
	case TerrainFileID::SWNRiver:
	case TerrainFileID::WNERiver:
	case TerrainFileID::NESRiver:
	{
		return Terrain::Type::River;
	}
	case TerrainFileID::HRoad:
	case TerrainFileID::VRoad:
	case TerrainFileID::CRoad:
	case TerrainFileID::ESRoad:
	case TerrainFileID::SWRoad:
	case TerrainFileID::WNRoad:
	case TerrainFileID::NERoad:
	case TerrainFileID::ESWRoad:
	case TerrainFileID::SWNRoad:
	case TerrainFileID::WNERoad:
	case TerrainFileID::NESRoad:
	{
		return Terrain::Type::Road;
	}
	case TerrainFileID::HBridge:
	case TerrainFileID::VBridge:
	{
		return Terrain::Type::Bridge;
	}
	case TerrainFileID::Sea:
	{
		return Terrain::Type::Sea;
	}
	case TerrainFileID::HShoal:
	case TerrainFileID::HShoalN:
	case TerrainFileID::VShoal:
	case TerrainFileID::VShoalE:
	{
		return Terrain::Type::Shoal;
	}
	case TerrainFileID::Reef:
	{
		return Terrain::Type::Reef;
	}
	case TerrainFileID::AcidRainAirport:
	case TerrainFileID::AmberBlazeAirport:
	case TerrainFileID::BlackHoleAirport:
	case TerrainFileID::BlueMoonAirport:
	case TerrainFileID::BrownDesertAirport:
	case TerrainFileID::CobaltIceAirport:
	case TerrainFileID::GreenEarthAirport:
	case TerrainFileID::GreySkyAirport:
	case TerrainFileID::JadeSunAirport:
	case TerrainFileID::NeutralAirport:
	case TerrainFileID::OrangeStarAirport:
	case TerrainFileID::PinkCosmosAirport:
	case TerrainFileID::PurpleLightningAirport:
	case TerrainFileID::TealGalaxyAirport:
	case TerrainFileID::RedFireAirport:
	case TerrainFileID::YellowCometAirport:
	case TerrainFileID::WhiteNovaAirport:
	{
		return Terrain::Type::Airport;
	}
	case TerrainFileID::AcidRainBase:
	case TerrainFileID::AmberBlazeBase:
	case TerrainFileID::BlackHoleBase:
	case TerrainFileID::BlueMoonBase:
	case TerrainFileID::BrownDesertBase:
	case TerrainFileID::CobaltIceBase:
	case TerrainFileID::GreenEarthBase:
	case TerrainFileID::GreySkyBase:
	case TerrainFileID::JadeSunBase:
	case TerrainFileID::NeutralBase:
	case TerrainFileID::OrangeStarBase:
	case TerrainFileID::PinkCosmosBase:
	case TerrainFileID::PurpleLightningBase:
	case TerrainFileID::RedFireBase:
	case TerrainFileID::TealGalaxyBase:
	case TerrainFileID::YellowCometBase:
	case TerrainFileID::WhiteNovaBase:
	{
		return Terrain::Type::Base;
	}
	case TerrainFileID::AcidRainCity:
	case TerrainFileID::AmberBlazeCity:
	case TerrainFileID::BlackHoleCity:
	case TerrainFileID::BlueMoonCity:
	case TerrainFileID::BrownDesertCity:
	case TerrainFileID::CobaltIceCity:
	case TerrainFileID::GreenEarthCity:
	case TerrainFileID::GreySkyCity:
	case TerrainFileID::JadeSunCity:
	case TerrainFileID::NeutralCity:
	case TerrainFileID::OrangeStarCity:
	case TerrainFileID::PinkCosmosCity:
	case TerrainFileID::PurpleLightningCity:
	case TerrainFileID::RedFireCity:
	case TerrainFileID::TealGalaxyCity:
	case TerrainFileID::YellowCometCity:
	case TerrainFileID::WhiteNovaCity:
	{
		return Terrain::Type::City;
	}
	case TerrainFileID::AcidRainComTower:
	case TerrainFileID::AmberBlazeComTower:
	case TerrainFileID::BlackHoleComTower:
	case TerrainFileID::BlueMoonComTower:
	case TerrainFileID::BrownDesertComTower:
	case TerrainFileID::CobaltIceComTower:
	case TerrainFileID::GreenEarthComTower:
	case TerrainFileID::GreySkyComTower:
	case TerrainFileID::JadeSunComTower:
	case TerrainFileID::NeutralComTower:
	case TerrainFileID::OrangeStarComTower:
	case TerrainFileID::PinkCosmosComTower:
	case TerrainFileID::PurpleLightningComTower:
	case TerrainFileID::RedFireComTower:
	case TerrainFileID::TealGalaxyComTower:
	case TerrainFileID::YellowCometComTower:
	case TerrainFileID::WhiteNovaComTower:
	{
		return Terrain::Type::ComTower;
	}
	case TerrainFileID::AcidRainLab:
	case TerrainFileID::AmberBlazeLab:
	case TerrainFileID::BlackHoleLab:
	case TerrainFileID::BlueMoonLab:
	case TerrainFileID::BrownDesertLab:
	case TerrainFileID::CobaltIceLab:
	case TerrainFileID::GreenEarthLab:
	case TerrainFileID::GreySkyLab:
	case TerrainFileID::JadeSunLab:
	case TerrainFileID::NeutralLab:
	case TerrainFileID::OrangeStarLab:
	case TerrainFileID::PinkCosmosLab:
	case TerrainFileID::PurpleLightningLab:
	case TerrainFileID::RedFireLab:
	case TerrainFileID::TealGalaxyLab:
	case TerrainFileID::YellowCometLab:
	case TerrainFileID::WhiteNovaLab:
	{
		return Terrain::Type::Lab;
	}
	case TerrainFileID::AcidRainPort:
	case TerrainFileID::AmberBlazePort:
	case TerrainFileID::BlackHolePort:
	case TerrainFileID::BlueMoonPort:
	case TerrainFileID::CobaltIcePort:
	case TerrainFileID::BrownDesertPort:
	case TerrainFileID::GreenEarthPort:
	case TerrainFileID::GreySkyPort:
	case TerrainFileID::JadeSunPort:
	case TerrainFileID::NeutralPort:
	case TerrainFileID::OrangeStarPort:
	case TerrainFileID::PinkCosmosPort:
	case TerrainFileID::PurpleLightningPort:
	case TerrainFileID::RedFirePort:
	case TerrainFileID::YellowCometPort:
	case TerrainFileID::TealGalaxyPort:
	case TerrainFileID::WhiteNovaPort:
	{
		return Terrain::Type::Port;
	}
	case TerrainFileID::AcidRainHeadquarters:
	case TerrainFileID::AmberBlazeHeadquarters:
	case TerrainFileID::BlackHoleHeadquarters:
	case TerrainFileID::BlueMoonHeadquarters:
	case TerrainFileID::CobaltIceHeadquarters:
	case TerrainFileID::BrownDesertHeadquarters:
	case TerrainFileID::GreenEarthHeadquarters:
	case TerrainFileID::GreySkyHeadquarters:
	case TerrainFileID::JadeSunHeadquarters:
	case TerrainFileID::OrangeStarHeadquarters:
	case TerrainFileID::PinkCosmosHeadquarters:
	case TerrainFileID::PurpleLightningHeadquarters:
	case TerrainFileID::RedFireHeadquarters:
	case TerrainFileID::TealGalaxyHeadquarters:
	case TerrainFileID::YellowCometHeadquarters:
	case TerrainFileID::WhiteNovaHeadquarters:
	{
		return Terrain::Type::Headquarters;
	}
	case TerrainFileID::HPipeSeam:
	case TerrainFileID::VPipeSeam:
	case TerrainFileID::VPipe:
	case TerrainFileID::HPipe:
	case TerrainFileID::NEPipe:
	case TerrainFileID::ESPipe:
	case TerrainFileID::SWPipe:
	case TerrainFileID::WNPipe:
	case TerrainFileID::NPipeEnd:
	case TerrainFileID::EPipeEnd:
	case TerrainFileID::SPipeEnd:
	case TerrainFileID::WPipeEnd:
	{
		return Terrain::Type::Pipe;
	}
	case TerrainFileID::MissileSilo:
	case TerrainFileID::MissileSiloEmpty:
	{
		return Terrain::Type::MissleSilo;
	}
	}
}