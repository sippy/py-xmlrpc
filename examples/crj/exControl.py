# an example Control class

import baseControl

exusage = '''
        echo    arg
                returns arg as a string

        echo2   arg1 arg2
                returns (arg1, arg2) as a string

	ident	arg
		returns arg
'''

# example Control class
#
class exControl(baseControl.baseControl):
	def __init__(self, logobj=None):
		baseControl.baseControl.__init__(self, exusage, logobj)

	def echo(self, arg):
		return str(arg)

	def echo2(self, arg1, arg2):
		return str((arg1, arg2))

	def ident(self, arg):
		return arg
