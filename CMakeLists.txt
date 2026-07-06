cmake_minimum_required(VERSION 3.23)
project(CKSP_Compiler VERSION 0.0.9)
set(PRE_RELEASE_VERSION "-alpha.10")

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(
        ${CMAKE_SOURCE_DIR}/version.h.in
        ${GENERATED_DIR}/version.h
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CLion/CMake can cache a Python path from an older local installation.
# If that interpreter no longer exists, drop the cached value and re-discover.
if(DEFINED Python3_EXECUTABLE AND NOT EXISTS "${Python3_EXECUTABLE}")
    unset(Python3_EXECUTABLE CACHE)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS -static) # Potential typeinfo/linking issues on Windows
endif()
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto -fno-plt -fomit-frame-pointer")

# Path to text files
set(TEXT_FILES_DIR "${CMAKE_SOURCE_DIR}/Builtins")

# Path to the directory where generated header files should be placed
set(GENERATED_HEADERS_DIR "${CMAKE_BINARY_DIR}/generated_headers")
file(MAKE_DIRECTORY ${GENERATED_HEADERS_DIR})

# Find all text files in the specified directory
file(GLOB TEXT_FILES CONFIGURE_DEPENDS "${TEXT_FILES_DIR}/*.txt")

foreach(TEXT_FILE ${TEXT_FILES})
    # Create a name for the generated header file
    get_filename_component(FILENAME ${TEXT_FILE} NAME_WE)
    set(HEADER_FILENAME "${GENERATED_HEADERS_DIR}/${FILENAME}.h")

    # Run a custom command to convert the file
    add_custom_command(
            OUTPUT ${HEADER_FILENAME}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/text_file_to_header.py ${TEXT_FILE} ${HEADER_FILENAME}
            DEPENDS ${TEXT_FILE} ${CMAKE_SOURCE_DIR}/scripts/text_file_to_header.py
            VERBATIM
    )

    # Add the generated header file to the source list
    list(APPEND GENERATED_HEADERS ${HEADER_FILENAME})
endforeach()
add_custom_target(generate_headers DEPENDS ${GENERATED_HEADERS})

add_library(cksp_core STATIC
        cksp/Tokenizer/Tokens.h
        cksp/Tokenizer/Tokenizer.h
        cksp/Tokenizer/Tokenizer.cpp
        cksp/Tokenizer/Token.h
        cksp/Parser/Parser.h
        cksp/Parser/Parser.cpp
        misc/Diagnostic.h
        misc/Diagnostic.cpp
        misc/DiagnosticReport.h
        misc/DiagnosticEngine.h
        misc/DiagnosticEngine.cpp
        misc/DiagnosticSink.h
        misc/DiagnosticSink.cpp
        misc/SourceLocation.h
        misc/Result.h
        cksp/Preprocessor/PreAST/PreASTNodes/PreAST.h
        cksp/Preprocessor/PreAST/PreASTNodes/PreAST.cpp
        cksp/Preprocessor/PreAST/PreASTVisitor.h
        cksp/Preprocessor/PreAST/PreASTVisitor.cpp
        cksp/Preprocessor/PreprocessorParser.h
        cksp/Preprocessor/PreprocessorParser.cpp
        cksp/Preprocessor/PreAST/PreASTMacros.h
        cksp/Preprocessor/PreAST/PreASTMacros.cpp
        cksp/ASTNodes/AST.h
        cksp/ASTNodes/AST.cpp
        cksp/ASTNodes/ASTDataStructures.h
        cksp/ASTNodes/ASTDataStructures.cpp
        cksp/ASTNodes/ASTHelper.h
        cksp/ASTNodes/ASTReferences.h
        cksp/ASTNodes/ASTReferences.cpp
        cksp/ASTVisitor/ASTVisitor.h
        cksp/ASTVisitor/ASTVisitor.cpp
        cksp/ASTVisitor/debug/ASTPrinter.h
        cksp/ASTVisitor/debug/ASTPrinter.cpp
        cksp/ASTVisitor/ASTDesugar.h
        cksp/ASTVisitor/ASTDesugar.cpp
        cksp/ASTVisitor/ASTCollectLowerings.h
        cksp/ASTVisitor/ASTCollectLowerings.cpp
        cksp/ASTVisitor/ASTSemanticAnalysis.h
        cksp/ASTVisitor/ASTSemanticAnalysis.cpp
        cksp/Types/Types.h
        cksp/Types/Types.cpp
        cksp/Types/TypeRegistry.h
        cksp/Types/TypeRegistry.cpp
        cksp/Interpreter/SimpleExprInterpreter.h
        cksp/Interpreter/SimpleExprInterpreter.cpp
        cksp/Preprocessor/PreAST/PreASTCombine.h
        cksp/Preprocessor/PreAST/PreASTIncrementer.h
        cksp/Preprocessor/PreAST/PreASTIncrementer.cpp
        cksp/BuiltinsProcessing/BuiltinsProcessor.h
        cksp/BuiltinsProcessing/BuiltinsProcessor.cpp
        cksp/Generator/ASTGenerator.h
        cksp/Generator/ASTGenerator.cpp
        ${GENERATED_HEADERS}
        generated/version.h
        cksp/Preprocessor/PreAST/PreASTDefines.h
        cksp/Preprocessor/PreAST/PreASTDefines.cpp
        cksp/Preprocessor/PreAST/PreASTPragma.h
        misc/FileHandler.h
        misc/FileHandler.cpp
        misc/CommandLineOptions.h
        cksp/BuiltinsProcessing/DefinitionProvider.h
        cksp/BuiltinsProcessing/DefinitionProvider.cpp
        cksp/Processor/Processor.h
        cksp/Processor/Processor.cpp
        misc/PathHandler.h
        misc/PathHandler.cpp
        cksp/Lowering/ASTLowering.h
        cksp/Lowering/LoweringList.h
        cksp/Lowering/DataLowering/DataLoweringNDArray.h
        cksp/Desugaring/DesugarFamily.h
        cksp/Desugaring/ASTDesugaring.h
        cksp/Desugaring/DesugarDeclareAssign.h
        cksp/Lowering/LoweringFor.h
        cksp/Lowering/LoweringForEach.h
        utils/Gensym.h
        utils/Timer.h
        cksp/Compiler.h
        cksp/ASTVisitor/ASTOptimizations.h
        cksp/ASTVisitor/ASTOptimizations.cpp
        cksp/ASTVisitor/ASTVariableChecking.h
        cksp/ASTVisitor/ASTVariableChecking.cpp
        cksp/ASTVisitor/TypeInference.h
        cksp/ASTVisitor/TypeInference.cpp
        cksp/Lowering/LoweringFunctionCall.h
        cksp/ASTNodes/ASTInstructions.h
        cksp/ASTNodes/ASTInstructions.cpp
        cksp/ASTVisitor/GlobalScope/ASTParameterPromotion.h
        cksp/ASTVisitor/GlobalScope/ASTVariableReuse.h
        cksp/ASTVisitor/GlobalScope/NormalizeArrayAssign.h
        cksp/Desugaring/DesugarStruct.h
        cksp/Lowering/LoweringStruct.h
        cksp/Lowering/LoweringPointer.h
        cksp/ASTVisitor/ASTLowerTypes.h
        cksp/Lowering/LoweringNil.h
        cksp/ASTVisitor/ReturnFunctionRewriting/ReturnFunctionCallHoisting.h
        cksp/ASTVisitor/ReturnFunctionRewriting/ReturnParamPromotion.h
        cksp/ASTVisitor/ASTDataStructureLowering.h
        cksp/ASTVisitor/ASTReturnFunctionRewriting.h
        cksp/Lowering/DataLowering/NDArrayCopyFunction.h
        cksp/ASTVisitor/NormalizeNDArrayAssign.h
        cksp/ASTVisitor/FunctionHandling/ASTFunctionInlining.h
        cksp/ASTVisitor/FunctionHandling/ASTPreemptiveFunctionInlining.h
        cksp/Optimization/ConstantPropagation.h
        cksp/Optimization/VariablePruning.h
        cksp/ASTVisitor/ASTRelinkGlobalScope.h
        cksp/Optimization/ConstExprValidator.h
        cksp/Optimization/ConstExprPropagation.h
        cksp/Optimization/DeadCodeElimination.h
        cksp/ASTVisitor/ASTKSPSyntaxCheck.h
        cksp/SyntaxChecks/KSPPersistency.h
        cksp/SyntaxChecks/KSPDeclarations.h
        misc/HashFunctions.h
        cksp/Optimization/ConstantPromotion.h
        cksp/Optimization/VarExistsValidator.h
        cksp/Optimization/MemoryExhaustedNesting.h
        cksp/Optimization/TokenCounter.h
        cksp/Desugaring/DesugarFunctionCall.h
        cksp/ASTVisitor/ASTHandleStringRepresentations.h
        cksp/Desugaring/DesugarDelete.h
        cksp/ASTVisitor/ASTPointerScope.h
        cksp/Desugaring/DesugarParamList.h
        cksp/Desugaring/DesugarBinaryExpr.h
        cksp/Lowering/LoweringWhile.h
        cksp/Lowering/LoweringFunctionDefReturnStmts.h
        cksp/ASTNodes/NodeStructCreateRefCountFunctions.h
        cksp/Lowering/PreLoweringStruct.h
        cksp/Lowering/LoweringMemAlloc.h
        cksp/Lowering/LoweringNumElements.h
        cksp/Desugaring/DesugarSingleAssignment.h
        cksp/Lowering/PostLowering/PostLoweringNumElements.h
        cksp/ASTVisitor/ASTCollectPostLowerings.h
        cksp/Optimization/NilValidator.h
        cksp/Lowering/LoweringUseCount.h
        cksp/ASTVisitor/ASTTypeAnnotations.h
        cksp/ASTVisitor/ReferenceManagement/ASTCollectReferences.h
        cksp/ASTVisitor/ReferenceManagement/ASTRemoveReferences.h
        misc/FreeFunctions.h
        cksp/ASTVisitor/FunctionHandling/FunctionDefinitionOrdering.h
        cksp/ASTVisitor/ReturnFunctionRewriting/ReturnFunctionIsolation.h
        cksp/ASTVisitor/FunctionHandling/FunctionInlining.h
        cksp/ASTVisitor/ArrayDimensionConstants.h
        cksp/Lowering/LoweringSortSearch.h
        cksp/Lowering/PostLowering/PostLoweringSortSearch.h
        cksp/ASTVisitor/GlobalScope/ASTDimensionExpansion.h
        cksp/Lowering/LoweringInitializerList.h
        cksp/ASTVisitor/ASTTemporaryPointerScope.h
        cksp/ASTVisitor/FunctionHandling/ReturnPathValidator.h
        cksp/ASTVisitor/GlobalScope/MarkThreadSafe.h
        cksp/ASTVisitor/FunctionHandling/ASTFunctionStrategy.h
        cksp/ASTVisitor/ReferenceManagement/ASTCollectDeclarations.h
        cksp/ASTVisitor/ReferenceManagement/ASTCollectCallSites.h
        cksp/ASTVisitor/GlobalScope/DeflateParameters.h
        cksp/ASTVisitor/FunctionHandling/BuiltinRestrictionValidator.h
        cksp/ASTVisitor/FunctionHandling/FunctionParamDataTypeGetter.h
        cksp/ASTVisitor/GlobalScope/DimensionDeflator.h
        cksp/Optimization/ArrayInitializationRaising.h
        cksp/Desugaring/DesugarUIControlArray.h
        cksp/ASTVisitor/UniqueArrayNamesProvider.h
        cksp/ASTVisitor/UniqueParameterNamesProvider.h
        cksp/ASTVisitor/FunctionHandling/ParameterAssignmentTransformation.h
        cksp/ASTVisitor/FunctionHandling/ParameterReuse.h
        cksp/Optimization/ReferenceValidator.h
        cksp/ASTVisitor/ASTNoVisitor.h
        cksp/ASTVisitor/FunctionHandling/UIControlParamHandling.h
        cksp/Desugaring/DesugarCompoundAssignment.h
        cksp/Desugaring/DesugarNamespace.h
        cksp/Lowering/LoweringRange.h
        cksp/ASTVisitor/FunctionHandling/FunctionShortCircuit.h
        cksp/Desugaring/DesugarFormatString.h
        cksp/Preprocessor/PreAST/PreASTNodes/PreASTHelper.h
        cksp/Preprocessor/PreAST/PreASTPrinter.h
        JSON/NCKPTranslator.cpp
        JSON/NCKPTranslator.h
        JSON/ast/JSONValue.cpp
        JSON/ast/JSONValue.h
        JSON/error/JSONError.h
        JSON/parser/JSONParser.h
        JSON/parser/JSONTokenizer.h
        JSON/parser/JSONTokens.h
        JSON/visitor/JSONVisitor.h
        JSON/visitor/JSONPrintVisitor.h
        cksp/Optimization/FreeVarCollector.h
        cksp/Lowering/LoweringBooleanExpression.h
        cksp/Lowering/LoweringTernaryOperator.h
        cksp/Lowering/LoweringBoolean.h
        cksp/SyntaxChecks/KSPConditions.h
        cksp/Lowering/LoweringComparisons.h
        cksp/Preprocessor/PreAST/PreASTImport.h
        cksp/Preprocessor/PreAST/PreASTConditions.h
        cksp/Optimization/VariableCollector.h
        cksp/Lowering/LoweringNDArray.h
        cksp/Optimization/ScalarVarToArray.h
        cksp/ASTVisitor/FunctionHandling/FunctionExitCheck.h
        cksp/ASTVisitor/ASTLifeTimeAnalysis.h
        cksp/ASTVisitor/ASTThreadSafeAnalysis.h
        cksp/ASTVisitor/ASTStructInstanceAnalysis.h
        cksp/Optimization/ConstantDatabase.h
)
add_dependencies(cksp_core generate_headers)

target_include_directories(cksp_core PUBLIC
        ${GENERATED_HEADERS_DIR}
        ${GENERATED_DIR}
)

target_compile_definitions(cksp_core PUBLIC PRINTER_OUTPUT_PATH="${CMAKE_SOURCE_DIR}")

add_executable(cksp
        main.cpp
        misc/CommandLineOptions.cpp
        cksp/CompilerConfig.h
)
target_link_libraries(cksp PRIVATE cksp_core)
