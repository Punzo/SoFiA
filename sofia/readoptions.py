#! /usr/bin/env python
import re
import sys
import traceback
import ast

def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        return False


def str2bool(s):
    if s in ['True', 'true', 'TRUE', 'Yes', 'yes', 'YES']:
        return True
    return False


def readPipelineOptions(filename = "pipeline.options"):
    f = open(filename, 'r')
    lines = f.readlines()
    f.close()
    
    # Remove leading/trailing whitespace and empty lines:
    lines = [line.strip() for line in lines]
    lines = [line for line in lines if len(line) > 0]
    lines2 = []
    
    # The following piece of code is meant to allow line breaks in parameter settings 
    # indicated by a double backwards slash (\\):
    for l in lines:
        if len(lines2) > 0:
            if lines2[-1][-1] == "\\":
                lines2[-1] = lines2[-1][:-1] + l
                continue
        lines2.append(l)
    lines = lines2
    tasks = {}
    
    # Define data types of individual parameters. This is used to 
    # convert the input values into the correct data type.
    # Ensure that all new parameters get added to this list!
    datatypes = {"steps.doSubcube": "bool", \
                 "steps.doFlag": "bool", \
                 "steps.doSmooth": "bool", \
                 "steps.doScaleNoise": "bool", \
                 "steps.doSCfind": "bool", \
                 "steps.doThreshold": "bool", \
                 "steps.doWavelet": "bool", \
                 "steps.doCNHI": "bool", \
                 "steps.doMerge": "bool", \
                 "steps.doReliability": "bool", \
                 "steps.doParameterise": "bool", \
                 "steps.doWriteFilteredCube": "bool", \
                 "steps.doWriteMask": "bool", \
                 "steps.doWriteCat": "bool", \
                 "steps.doMom0": "bool", \
                 "steps.doMom1": "bool", \
                 "steps.doCubelets": "bool", \
                 "steps.doDebug": "bool", \
                 "steps.doOptical": "bool", \
                 "import.inFile": "string", \
                 "import.weightsFile": "string", \
                 "import.maskFile": "string", \
                 "import.weightsFunction": "string", \
                 "import.subcubeMode": "string", \
                 "import.subcube": "array", \
                 "flag.regions": "array", \
                 "optical.sourceCatalogue": "string", \
                 "optical.spatSize": "float", \
                 "optical.specSize": "float", \
                 "optical.storeMultiCat": "bool", \
                 "smooth.kernel": "string", \
                 "smooth.edgeMode": "string", \
                 "smooth.kernelX": "float", \
                 "smooth.kernelY": "float", \
                 "smooth.kernelZ": "float", \
                 "scaleNoise.statistic": "string", \
                 "scaleNoise.edgeX": "int", \
                 "scaleNoise.edgeY": "int", \
                 "scaleNoise.edgeZ": "int", \
                 "scaleNoise.scaleX": "bool", \
                 "scaleNoise.scaleY": "bool", \
                 "scaleNoise.scaleZ": "bool", \
                 "SCfind.threshold": "float", \
                 "SCfind.sizeFilter": "float", \
                 "SCfind.maskScaleXY": "float", \
                 "SCfind.maskScaleZ": "float", \
                 "SCfind.edgeMode": "string", \
                 "SCfind.rmsMode": "string", \
                 "SCfind.kernels": "array", \
                 "SCfind.kernelUnit": "string", \
                 "SCfind.verbose": "bool", \
                 "CNHI.pReq": "float", \
                 "CNHI.qReq": "float", \
                 "CNHI.minScale": "int", \
                 "CNHI.maxScale": "int", \
                 "CNHI.medianTest": "bool", \
                 "CNHI.verbose": "int", \
                 "wavelet.threshold": "float", \
                 "wavelet.scaleXY": "int", \
                 "wavelet.scaleZ": "int", \
                 "wavelet.positivity": "bool", \
                 "wavelet.iterations": "int", \
                 "threshold.threshold": "float", \
                 "threshold.clipMethod": "string", \
                 "threshold.rmsMode": "string", \
                 "threshold.verbose": "bool", \
                 "merge.radiusX": "int", \
                 "merge.radiusY": "int", \
                 "merge.radiusZ": "int", \
                 "merge.minSizeX": "int", \
                 "merge.minSizeY": "int", \
                 "merge.minSizeZ": "int", \
                 "reliability.parSpace": "array", \
                 "reliability.kernel": "array", \
                 "reliability.fMin": "float", \
                 "reliability.threshold": "float", \
                 "reliability.makePlot": "bool", \
                 "parameters.fitBusyFunction": "bool", \
                 "parameters.optimiseMask": "bool", \
                 "parameters.dilateMask": "bool", \
                 "parameters.dilateThreshold":"float",\
                 "parameters.dilatePixMax":"int",\
                 "parameters.dilateChan":"int",\
                 "writeCat.overwrite":"bool", \
                 "writeCat.compress":"bool", \
                 "writeCat.outputDir": "string", \
                 "writeCat.basename": "string", \
                 "writeCat.writeASCII": "bool", \
                 "writeCat.writeXML": "bool", \
                 "writeCat.writeSQL": "bool", \
                 "writeCat.parameters": "array"
    }
    
    # Loop through all lines:
    for linenr, line in enumerate(lines):
        if line[0] == "#":
            continue
        
        try:
            parameter, value = tuple(line.split("=", 1))
            if len(parameter) < 1:
                sys.stderr.write("WARNING: Option name missing in line {0:02d}: {1:s}\n".format(linenr + 1, line))
                continue
            
            subtasks = tasks
            
            while True:
                module = parameter.split(".")[0]
                module = module.strip()
                
                if len(module) < 1:
                    sys.stderr.write("WARNING: (Sub)key name too short in line {0:02d}: {1:s}\n".format(linenr + 1, line))
                    break
                
                parameter = str(".").join(parameter.split(".")[1:])
                parameter = parameter.strip()
                
                if not subtasks.has_key(module):
                    subtasks[module] = {}
                subtasks = subtasks[module]
                
                if parameter.count(".") == 0:
                    if subtasks.has_key(parameter):
                        sys.stderr.write("WARNING: Option already present in line {0:02d}: {1:s}\n".format(linenr + 1, line))
                        break
                    try:
                        value = value.split('#')[0].strip()
                        searchKey = module + "." + parameter
                        
                        if searchKey in datatypes:
                            if datatypes[searchKey] == "bool":
                                subtasks[parameter] = str2bool(value)
                            elif datatypes[searchKey] == "float":
                                subtasks[parameter] = float(value)
                            elif datatypes[searchKey] == "int":
                                subtasks[parameter] = int(value)
                            elif datatypes[searchKey] == "array":
                                subtasks[parameter] = ast.literal_eval(value)
                            else:
                                subtasks[parameter] = str(value)
                        else:
                            sys.stderr.write("WARNING: No data type defined for parameter {0:s}.\n".format(searchKey))
                            sys.stderr.write("         Guessing type based on value.\n")
                            subtasks[parameter] = ast.literal_eval(value)
                    except:
                        sys.stderr.write("WARNING: Could not parse option in line {0:02d}: {1:s}\n".format(linenr + 1, line))
                        break
                    break
        except:
            pass
    
    return tasks
