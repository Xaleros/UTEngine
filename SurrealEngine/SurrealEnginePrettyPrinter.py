###################################################################################################
# Add this to your gdbinit
###################################################################################################

import gdb.printing

class SeValuePrinter(gdb.ValuePrinter):
    def __init__(self, val):
        self.val = val

class SeArrayPrinter(SeValuePrinter):
    def to_string(self):
        return "{_size = " + str(self.val['_size']) + ", _capacity = " + str(self.val['_capacity']) + "}"

class SeNameStringPrinter(SeValuePrinter):
    def to_string(self):
        return gdb.execute("p NameString::Names[" + str(self.val['SpelledIndex']) + "]", to_string=True).split(" ")[-1].strip()

class SePackagePrinter(SeValuePrinter):
    def to_string(self):
        return "{Name = " + str(self.val['Name']) + ", Filename = " + str(self.val['Filename']) + "}"

class SeVec2TPrinter(SeValuePrinter):
    def to_string(self):
        return "{x =" + str(self.val['x']) + ", y = " + str(self.val['y']) + "}"

class SeVec3TPrinter(SeValuePrinter):
    def to_string(self):
        return "{x =" + str(self.val['x']) + ", y = " + str(self.val['y']) + ", z = " + str(self.val['z']) + "}"

class SeVec4TPrinter(SeValuePrinter):
    def to_string(self):
        return "{x =" + str(self.val['x']) + ", y = " + str(self.val['y']) + ", z = " + str(self.val['z']) + ", w = " + str(self.val['w']) + "}"

class SeColorPrinter(SeValuePrinter):
    def to_string(self):
        return "{R =" + str(self.val['R']) + ", G = " + str(self.val['G']) + ", B = " + str(self.val['B']) + ", A = " + str(self.val['A']) + "}"

class SeRotatorPrinter(SeValuePrinter):
    def to_string(self):
        return "{Pitch =" + str(self.val['Pitch']) + ", Yaw = " + str(self.val['Yaw']) + ", Roll = " + str(self.val['Roll']) + "}"

###################################################################################################
# UObject printers
###################################################################################################

class SeUObjectPrinter(SeValuePrinter):
    def class_name(self):
        return str(self.val['Class']['Name'])

    def name_str(self):
        return "Name = " + str(self.val['Name'])

    def class_str(self):
        return "Class = " + self.class_name()

    def package_str(self):
        return "Package = " + str(self.val['package']['Name'])

    def to_string(self):
        return "{" + self.name_str() + ", " + self.class_str() + ", " + self.package_str() + "}"

class SeUFieldPrinter(SeUObjectPrinter):
    def class_name(self):
        return "UField"

class SeUConstPrinter(SeUObjectPrinter):
    def class_name(self):
        return "UConst"

    def to_string(self):
        return "{" + self.name_str() + ", " + self.class_str() + ", " + self.package_str() + ", Constant = " + str(self.val['Constant']) + "}"

class SeUEnumPrinter(SeUObjectPrinter):
    def class_name(self):
        return "UEnum"

    def to_string(self):
        # FIXME: would be nice to print out all the enum names here
        return "{" + self.name_str() + ", " + self.class_str() + ", ElementNames = " + str(self.val['ElementNames']) + "}"

class SeUStructPrinter(SeUObjectPrinter):
    def class_name(self):
        return "UStruct"

    def to_string(self):
        return "{" + self.name_str() + ", " + self.class_str() + ", BaseStruct = " + str(self.val['BaseStruct']) + "}"

class SeUFunctionPrinter(SeUStructPrinter):
    def class_name(self):
        return "UFunction"

# FIXME: print all functions in the state here
class SeUStatePrinter(SeUStructPrinter):
    def class_name(self):
        return "UState"

class SeUClassPrinter(SeUStructPrinter):
    def class_name(self):
        return "UClass"

def BuildSurrealEnginePrettyPrinter():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("SurrealEngine")
    pp.add_printer('Array',      '^Array<*>$',   SeArrayPrinter)
    pp.add_printer('Color',      '^Color$',      SeColorPrinter)
    pp.add_printer('NameString', '^NameString$', SeNameStringPrinter)
    pp.add_printer('Package',    '^Package$',    SeNameStringPrinter)
    pp.add_printer('Rotator',    '^Rotator$',    SeRotatorPrinter)
    pp.add_printer('vec2T',      '^vec2T<*>$',   SeVec2TPrinter)
    pp.add_printer('vec3T',      '^vec3T<*>$',   SeVec3TPrinter)
    pp.add_printer('vec4T',      '^vec4T<*>$',   SeVec4TPrinter)

    pp.add_printer('UObject',   '^UObject$',   SeUObjectPrinter)
    pp.add_printer('UField',    '^UField$',    SeUFieldPrinter)
    pp.add_printer('UConst',    '^UConst$',    SeUConstPrinter)
    pp.add_printer('UEnum',     '^UEnum$',     SeUEnumPrinter)
    pp.add_printer('UStruct',   '^UStruct$',   SeUStructPrinter)
    pp.add_printer('UFunction', '^UFunction$', SeUFunctionPrinter)
    pp.add_printer('UState',    '^UState$',    SeUStatePrinter)
    pp.add_printer('UClass',    '^UClass$',    SeUClassPrinter)
    return pp

gdb.printing.register_pretty_printer(gdb.current_objfile(), BuildSurrealEnginePrettyPrinter())