--
--  Custom Options Definition Table format
--
--  NOTES:
--  - using an enumerated table lets you specify the options order
--
--  These keywords must be lowercase for LuaParser to read them.
--
--  key:      the string used in the script.txt
--  name:     the displayed name
--  desc:     the description (could be used as a tooltip)
--  type:     the option type
--  def:      the default value;
--  min:      minimum value for number options
--  max:      maximum value for number options
--  step:     quantization step, aligned to the def value
--  maxlen:   the maximum string length for string options
--  items:    array of item strings for list options
--  scope:    'all', 'player', 'team', 'allyteam'      <<< not supported yet >>>
--
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

local options = {
	{ -- section
		key    = 'performance',
		name   = 'Performance relevant settings',
		desc   = 'These settings may be relevant for both CPU usage and AI difficulty.',
		type   = 'section',
	},
	
	{ -- list
		section = 'performance',
		key     = 'difficulty',
		name    = 'Difficulty level',
		desc    = 'Customize difficulty level',
		type    = 'list',
		def     = '3',
		items   = {
			{
				key  = '1',
				name = 'Easy',
			},
			
			{
				key  = '2',
				name = 'Normal',
			},
			
			{
				key  = '3',
				name = 'Hard',
			},
		},
	},
	
	{ -- list
		section = 'performance',
		key     = 'logging',
		name    = 'Log level',
		desc    = 'Specifies how much information to output into log file',
		type    = 'list',
		def     = '0',
		items   = {
			{
				key  = '0',
				name = 'Nothing',
			},
			
			{
				key  = '1',
				name = 'Errors',
			},
			{
				key  = '2',
				name = 'Warnings',
			},
			{
				key  = '3',
				name = 'Verbose',
			},
		},
	},
}

return options
