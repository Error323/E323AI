--
--  Info Definition Table format
--
--
--  These keywords must be lowercase for LuaParser to read them.
--
--  key:      user defined or one of the SKIRMISH_AI_PROPERTY_* defines in
--            SSkirmishAILibrary.h
--  value:    the value of the property
--  desc:     the description (could be used as a tooltip)
--
--
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

local infos = {
	{
		key    = 'shortName',
		value  = 'E323AI',
		desc   = 'machine conform name.',
	},
	{
		key    = 'version',
		value  = '3.18.0', -- AI version - !This comment is used for parsing!
	},
	{
		key    = 'name',
		value  = 'E323AI - High Templar',
		desc   = 'Human readable name',
	},
	{
		key    = 'description',
		value  = 'Configurable Guerilla AI - Not supporting watermaps yet. Beats all other AIs',
		desc   = 'tooltip.',
	},
	{
		key    = 'url',
		value  = 'http://springrts.com/phpbb/viewtopic.php?f=15&t=19450',
		desc   = 'URL with info on the AI in spring forum',
	},
	{
		key    = 'loadSupported',
		value  = 'no',
		desc   = 'whether this AI supports loading or not',
	},
	{
		key    = 'interfaceShortName',
		value  = 'C', -- AI Interface name - !This comment is used for parsing!
		desc   = 'the shortName of the AI interface this AI needs.',
	},
	{
		key    = 'interfaceVersion',
		value  = '0.1', -- AI Interface version - !This comment is used for parsing!
		desc   = 'the minimum version of the AI interface this AI needs.',
	},
}

return infos
