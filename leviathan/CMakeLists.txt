# A minimal Leviathan plugin build file

set(CREATE_STATIC ON)

set(GroupCore "Define.cpp" "Define.h" "FileSystem.cpp" "FileSystem.h" "ForwardDeclarations.h"
  "ErrorReporter.h" "Logger.cpp" "Logger.h" "utf8.h" "Include.h"
  "Exceptions.h" "Exceptions.cpp"
  "TimeIncludes.h" "TimeIncludes.cpp" "WindowsInclude.h"
  )

set(GroupCommon "Common/BaseNotifiable.h" "Common/BaseNotifiableImpl.h"
  "Common/BaseNotifier.h" "Common/BaseNotifierImpl.h"
  "Common/ReferenceCounted.h"
  "Common/StringOperations.cpp" "Common/StringOperations.h"
  "Common/ThreadSafe.h" 
  "Common/Types.h" "Common/Types.cpp"
  "Common/Visitor.cpp" "Common/Visitor.h"
  )

set(GroupCommonData "Common/DataStoring/DataBlock.cpp" "Common/DataStoring/DataBlock.h"
  "Common/DataStoring/NamedVars.cpp" "Common/DataStoring/NamedVars.h"
  )

set(GroupEntities "Entities/EntityCommon.h")

set(GroupUtility "Utility/Convert.cpp" "Utility/Convert.h")

file(GLOB GroupIterators
  "Iterators/IteratorData.h"
  "Iterators/StringDataIterator.cpp" "Iterators/StringDataIterator.h"
  "Iterators/StringIterator.cpp" "Iterators/StringIterator.h"
  )

set(GroupObjectFiles
  "ObjectFiles/ObjectFile.cpp" "ObjectFiles/ObjectFile.h"
  "ObjectFiles/ObjectFileProcessor.cpp" "ObjectFiles/ObjectFileProcessor.h"
  )

# collect all files to one group
set(EngineFiles ${GroupCore} ${GroupEntities} ${GroupUtility} ${GroupStatistics}
  ${GroupMath} ${GroupSound}
    ${GroupScript} ${GroupGUI}  ${GroupEvents}
    ${GroupInput} ${GroupHandlers} ${GroupCommon} ${GroupApplication} ${GroupExceptions}
    ${GroupObjectFiles} ${GroupRendering} ${GroupCommonData} ${GroupUtilityIte}
    ${GroupUtilityFile}
    ${GroupScriptAddons} ${GroupRenderingGUI} ${GroupNewton} ${GroupEntitiesObjects}
    ${GroupUtilityData} ${GroupAddons} ${GroupNetworking} ${GroupThreading} ${GroupIterators}
    ${GroupGameplay} 
    ${GroupPermissions} ${GroupEntitiesSerializers} ${GroupLZ4} ${GroupJsoncpp}
    ${GeneratorInput} ${GroupGeneratedFiles}
    ${Grouputf8})
    
# engine exporting define
add_definitions(-DENGINE_EXPORTS)
add_definitions(-DLEVIATHAN_BUILD)
add_definitions(-DNO_DEFAULT_DATAINDEX)

# create library with all the files
if(CREATE_STATIC)
    add_library(Leviathan ${EngineFiles})
else()
    add_library(Leviathan SHARED ${EngineFiles})
endif()
