# This file was generated by fedex_python.  You probably don't want to edit
# it since your modifications will be lost if fedex_plus is used to
# regenerate it.
from SCL.SCLBase import *
from SCL.SimpleDataTypes import *
from SCL.ConstructedDataTypes import *
from SCL.AggregationDataTypes import *
from SCL.TypeChecker import check_type
from SCL.Expr import *
maths_number = NUMBER
# SELECT TYPE atom_based_value_
if (not 'maths_number' in globals().keys()):
	maths_number = 'maths_number'
if (not 'atom_based_tuple' in globals().keys()):
	atom_based_tuple = 'atom_based_tuple'
atom_based_value = SELECT(
	'maths_number',
	'atom_based_tuple')
atom_based_tuple = LIST(0,None,'atom_based_value')
