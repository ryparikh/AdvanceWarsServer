#pragma once

class Map;

class MapView
{
public:
	MapView(const Map* pMap) :
		m_pMap(pMap)
	{
	}

	void Render();
private:
	const Map* m_pMap{ nullptr };
	bool showCursor = false;
	int x = 1;
	int y = 1;
};
