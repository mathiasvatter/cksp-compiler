cmake_minimum_required(VERSION 3.23)
project(CKSP_Compiler VERSION 0.0.7)
set(PRE_RELEASE_VERSION "-alpha.1")

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(
        ${CMAKE_SOURCE_DIR}/version.h.in
        ${GENERATED_DIR}/version.h
)

set(CMAKE_CXX_STANDARD 23)
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS -static)
endif()
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto -fno-plt -fomit-frame-pointer")

# Pfad zu den Textdateien
set(TEXT_FILES_DIR "${CMAKE_SOURCE_DIR}/Builtins")

# Pfad zum Verzeichnis, in dem die generierten Header-Dateien abgelegt werden sollen
set(GENERATED_HEADERS_DIR "${CMAKE_BINARY_DIR}/generated_headers")
file(MAKE_DIRECTORY ${GENERATED_HEADERS_DIR})

# Finden Sie alle Textdateien im angegebenen Verzeichnis
file(GLOB TEXT_FILES "${TEXT_FILES_DIR}/*.txt")

# Für jede Textdatei
foreach(TEXT_FILE ${TEXT_FILES})
    # Erstellen Sie einen Namen für die generierte Header-Datei
    get_filename_component(FILENAME ${TEXT_FILE} NAME_WE)
    set(HEADER_FILENAME "${GENERATED_HEADERS_DIR}/${FILENAME}.h")

    # Führen Sie ein benutzerdefiniertes Kommando aus, um die Datei zu konvertieren
    add_custom_command(
            OUTPUT ${HEADER_FILENAME}
            COMMAND python3 ${CMAKE_SOURCE_DIR}/text_file_to_header.py ${TEXT_FILE} ${HEADER_FILENAME}
            DEPENDS ${TEXT_FILE}
    )

    # Fügen Sie die generierte Header-Datei zur Liste der Quelldateien hinzu
    list(APPEND GENERATED_HEADERS ${HEADER_FILENAME})
endforeach()

include_directories(${GENERATED_HEADERS_DIR}
        ${GENERATED_DIR})

add_executable(cksp main.cpp
        Tokenizer/Tokens.h
        Tokenizer/Tokenizer.h
        Tokenizer/Tokenizer.cpp
        Parser/Parser.h
        Parser/Parser.cpp
        misc/CompileError.h
        misc/CompileError.cpp
        misc/Result.h
        Preprocessor/Preprocessor.h
        Preprocessor/Preprocessor.cpp
        Preprocessor/ImportProcessor.h
        Preprocessor/ImportProcessor.cpp
        Preprocessor/PreprocessorConditions.h
        Preprocessor/PreprocessorConditions.cpp
        Preprocessor/PreAST/PreAST.h
        Preprocessor/PreAST/PreAST.cpp
        Preprocessor/PreAST/PreASTVisitor.h
        Preprocessor/PreAST/PreASTVisitor.cpp
        Preprocessor/PreprocessorParser.h
        Preprocessor/PreprocessorParser.cpp
        Preprocessor/PreAST/PreASTDesugar.h
        Preprocessor/PreAST/PreASTDesugar.cpp
        AST/ASTNodes/AST.h
        AST/ASTNodes/AST.cpp
        AST/ASTNodes/ASTDataStructures.h
        AST/ASTNodes/ASTDataStructures.cpp
        AST/ASTNodes/ASTHelper.h
        AST/ASTNodes/ASTReferences.h
        AST/ASTNodes/ASTReferences.cpp
        AST/ASTVisitor/ASTVisitor.h
        AST/ASTVisitor/ASTVisitor.cpp
        AST/ASTVisitor/ASTPrinter.h
        AST/ASTVisitor/ASTPrinter.cpp
        AST/ASTVisitor/ASTDesugar.h
        AST/ASTVisitor/ASTDesugar.cpp
        AST/ASTVisitor/ASTCollectLowerings.h
        AST/ASTVisitor/ASTCollectLowerings.cpp
        AST/ASTVisitor/ASTSemanticAnalysis.h
        AST/ASTVisitor/ASTSemanticAnalysis.cpp
        AST/Types.h
        AST/Types.cpp
        AST/TypeRegistry.h
        AST/TypeRegistry.cpp
        Interpreter/SimpleExprInterpreter.h
        Interpreter/SimpleExprInterpreter.cpp
        Preprocessor/PreAST/PreASTCombine.h
        Preprocessor/PreAST/PreASTCombine.cpp
        Preprocessor/PreAST/PreASTIncrementer.h
        Preprocessor/PreAST/PreASTIncrementer.cpp
        BuiltinsProcessing/BuiltinsProcessor.h
        BuiltinsProcessing/BuiltinsProcessor.cpp
        Generator/ASTGenerator.h
        Generator/ASTGenerator.cpp
        ${GENERATED_HEADERS}
        generated/version.h
        JSON/JSONParser.h
        JSON/JSONParser.cpp
        JSON/JSONVisitor.h
        JSON/JSONVisitor.cpp
        Preprocessor/PreAST/PreASTDefines.h
        Preprocessor/PreAST/PreASTDefines.cpp
        Preprocessor/PreAST/PreASTPragma.h
        Preprocessor/PreAST/PreASTPragma.cpp
        misc/FileHandler.h
        misc/FileHandler.cpp
        misc/CommandLineOptions.h
        misc/CommandLineOptions.cpp
        BuiltinsProcessing/DefinitionProvider.h
        BuiltinsProcessing/DefinitionProvider.cpp
        Processor/Processor.h
        Processor/Processor.cpp
        misc/PathHandler.h
        misc/PathHandler.cpp
        Lowering/ASTLowering.h
        Lowering/LoweringList.h
        Lowering/LoweringNDArray.h
        Lowering/LoweringUIControlArray.h
        Desugaring/DesugaringFamily.h
        Desugaring/ASTDesugaring.h
        Desugaring/DesugarDeclareAssign.h
        Desugaring/DesugarFor.h
        Desugaring/DesugarForEach.h
        misc/Gensym.h
        misc/Timer.h
        Compiler.h
        Compiler.cpp
        AST/ASTVisitor/ASTOptimizations.h
        AST/ASTVisitor/ASTOptimizations.cpp
        AST/ASTVisitor/ASTVariableChecking.h
        AST/ASTVisitor/ASTVariableChecking.cpp
        AST/ASTVisitor/GlobalScope/ASTGlobalScope.h
        AST/ASTVisitor/GlobalScope/ASTGlobalScope.cpp
        AST/ASTVisitor/TypeInference.h
        AST/ASTVisitor/TypeInference.cpp
        Lowering/LoweringFunctionCall.h
        AST/ASTNodes/ASTInstructions.h
        AST/ASTNodes/ASTInstructions.cpp
        AST/ASTVisitor/GlobalScope/ASTParameterPromotion.h
        AST/ASTVisitor/GlobalScope/ASTRegisterReuse.h
        AST/ASTVisitor/GlobalScope/NormalizeArrayAssign.h
        Desugaring/DesugarStruct.h
        Lowering/LoweringStruct.h
        Lowering/LoweringPointer.h
        AST/ASTVisitor/ASTLowerTypes.h
        Lowering/LoweringNil.h
        Optimization/FunctionCallHoisting.h
        AST/ASTVisitor/ASTReturnParamPromotion.h
        AST/ASTVisitor/ASTDataStructureLowering.h
        AST/ASTVisitor/ASTReturnFunctionRewriting.h
        Lowering/NDArrayCopyFunction.h
        AST/ASTVisitor/NormalizeNDArrayAssign.h
        AST/ASTVisitor/ASTFunctionInlining.h
        AST/ASTVisitor/ASTExpressionFunctionInlining.h
        Optimization/ConstantPropagation.h
        Optimization/VariablePruning.h
        Optimization/ParameterMarking.h
        AST/ASTVisitor/ASTRelinkGlobalScope.h
        Optimization/ConstExprValidator.h
        Optimization/ConstExprPropagation.h
        Optimization/DeadCodeElimination.h
        AST/ASTVisitor/ASTKSPSyntaxCheck.h
        SyntaxChecks/KSPPersistency.h
        SyntaxChecks/KSPDeclarations.h
        misc/HashFunctions.h
        Optimization/ConstantPromotion.h
        Optimization/VarExistsValidator.h
        Optimization/MemoryExhaustedNesting.h
        Optimization/TokenCounter.h
        Desugaring/DesugarFunctionCall.h
        AST/ASTVisitor/ASTHandleStringRepresentations.h
        Desugaring/DesugarDelete.h
        AST/ASTVisitor/ASTReferenceCounting.h
        Desugaring/DesugarParamList.h
        AST/ASTVisitor/ASTInitializerFunctionInlining.h
        Desugaring/DesugarBinaryExpr.h
        Lowering/LoweringWhile.h
        Lowering/LoweringFunctionDef.h
        AST/ASTNodes/NodeStructCreateRefCountFunctions.h
        Lowering/PreLoweringStruct.h
)