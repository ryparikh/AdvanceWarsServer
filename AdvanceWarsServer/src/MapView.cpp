#include "MapView.h"

#include "Map.h"
#include "Terrain.h"
#include "Unit.h"
#include <iostream>

void MapView::Render()
{
	int rows = static_cast<int>(m_pMap->GetRows());
	int cols = static_cast<int>(m_pMap->GetCols());
	for (int y = 0; y < rows; ++y)
	{
		for (int x = 0; x < cols; ++x)
		{
			if (showCursor && x == 0 && y == 0)
			{
				std::cout << static_cast<char>(178);
				continue;
			}
			Terrain terrain;
			const Unit* pUnit{ nullptr };
			m_pMap->TryGetUnit(x, y, &pUnit);
			if (pUnit != nullptr)
			{
				std::cout << "_";
				continue;
			}

			m_pMap->TryGetTerrain(x, y, terrain);
			switch (terrain.m_type)
			{
			case Terrain::Type::Airport:
			{
				std::cout << "A";
				break;
			}
			case Terrain::Type::Base:
			{
				std::cout << "B";
				break;
			}
			case Terrain::Type::Bridge:
			{
				std::cout << "b";
				break;
			}
			case Terrain::Type::City:
			{
				std::cout << "C";
				break;
			}
			case Terrain::Type::ComTower:
			{
				std::cout << "T";
				break;
			}
			case Terrain::Type::Forest:
			{
				std::cout << "F";
				break;
			}
			case Terrain::Type::Headquarters:
			{
				std::cout << "H";
				break;
			}
			case Terrain::Type::Lab:
			{
				std::cout << "L";
				break;
			}
			case Terrain::Type::MissleSilo:
			{
				std::cout << "m";
				break;
			}
			case Terrain::Type::Mountain:
			{
				std::cout << "M";
				break;
			}
			case Terrain::Type::Pipe:
			{
				std::cout << "p";
				break;
			}
			case Terrain::Type::Plain:
			{
				std::cout << "P";
				break;
			}
			case Terrain::Type::Port:
			{
				std::cout << "O";
				break;
			}
			case Terrain::Type::Reef:
			{
				std::cout << "E";
				break;
			}
			case Terrain::Type::River:
			{
				std::cout << "R";
				break;
			}
			case Terrain::Type::Road:
			{
				std::cout << "r";
				break;
			}
			case Terrain::Type::Sea:
			{
				std::cout << "s";
				break;
			}
			case Terrain::Type::Shoal:
			{
				std::cout << "S";
				break;
			}
			}

		}
		std::cout << "\n";
	}
	showCursor = !showCursor;
}