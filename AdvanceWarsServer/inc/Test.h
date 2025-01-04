#pragma once

auto test = R"({
	"activePlayer":0,
	"gameId" : "c111b216-187e-47a6-a45c-2c6b0b9675b3",
	"map" : [
		[{"terrain":15}, {"terrain":15}, {"terrain":15}],
		[{"terrain":15}, {"terrain":15, "unit":{"ammo":0,"fuel":99,"health":10,"owner":"orange-star","type":"infantry"}}, {"terrain":15, "unit":{"ammo":0,"fuel":99,"health":10,"owner":"blue-moon","type":"infantry"}}],
		[{"terrain":15}, {"terrain":15}, {"terrain":15}]
	],
	"players" : [{"armyType":"orange-star", "co" : "Andy", "funds" : 3000, "power-meter" : [0, 54000], "power-status" : 0}, { "armyType":"blue-moon","co" : "Adder","funds" : 0,"power-meter" : [0,45000],"power-status" : 0 }]})";
