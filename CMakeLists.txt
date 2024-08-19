cmake_minimum_required(VERSION 3.23)
project(CKSP_Compiler VERSION 0.0.6)

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

add_executable(cksp main.cpp Tokenizer/Tokenizer.cpp Tokenizer/Tokenizer.h
        Tokenizer/Tokens.h
        Tokenizer/Tokenizer.h
        Tokenizer/Tokenizer.cpp
        Parser/Parser.h
        Parser/Parser.cpp
        misc/CompileError.cpp
        misc/CompileError.h
        misc/Result.h
        Preprocessor/Preprocessor.cpp
        Preprocessor/Preprocessor.h
        Preprocessor/ImportProcessor.cpp
        Preprocessor/ImportProcessor.h
        Preprocessor/PreprocessorConditions.cpp
        Preprocessor/PreprocessorConditions.h
        Preprocessor/PreAST/PreAST.h
        Preprocessor/PreAST/PreAST.cpp
        Preprocessor/PreAST/PreASTVisitor.h
        Preprocessor/PreAST/PreASTVisitor.cpp
        Preprocessor/PreprocessorParser.cpp
        Preprocessor/PreprocessorParser.h
        Preprocessor/PreAST/PreASTDesugar.cpp
        Preprocessor/PreAST/PreASTDesugar.h
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
        AST/ASTVisitor/ASTDesugar.cpp
        AST/ASTVisitor/ASTDesugar.h
        AST/ASTVisitor/ASTCollectLowerings.cpp
        AST/ASTVisitor/ASTCollectLowerings.h
        AST/ASTVisitor/ASTSemanticAnalysis.cpp
        AST/ASTVisitor/ASTSemanticAnalysis.h
        AST/Types.cpp
        AST/Types.h
        AST/TypeRegistry.cpp
        AST/TypeRegistry.h
        Interpreter/SimpleExprInterpreter.cpp
        Interpreter/SimpleExprInterpreter.h
        Preprocessor/PreAST/PreASTCombine.cpp
        Preprocessor/PreAST/PreASTCombine.h
        Preprocessor/PreAST/PreASTIncrementer.cpp
        Preprocessor/PreAST/PreASTIncrementer.h
        BuiltinsProcessing/BuiltinsProcessor.cpp
        BuiltinsProcessing/BuiltinsProcessor.h
        Generator/ASTGenerator.cpp
        Generator/ASTGenerator.h
        ${GENERATED_HEADERS}
        generated/version.h
        JSON/JSONParser.h
        JSON/JSONParser.cpp
        JSON/JSONVisitor.h
        JSON/JSONVisitor.cpp
        Preprocessor/PreAST/PreASTDefines.cpp
        Preprocessor/PreAST/PreASTDefines.h
        Preprocessor/PreAST/PreASTPragma.cpp
        Preprocessor/PreAST/PreASTPragma.h
        misc/FileHandler.cpp
        misc/FileHandler.h
        misc/CommandLineOptions.cpp
        misc/CommandLineOptions.h
        BuiltinsProcessing/DefinitionProvider.h
        BuiltinsProcessing/DefinitionProvider.cpp
        Processor/Processor.cpp
        Processor/Processor.h
        misc/PathHandler.cpp
        misc/PathHandler.h
        Lowering/ASTLowering.h
        Lowering/LoweringList.h
        Lowering/LoweringNDArray.h
        Lowering/LoweringUIControlArray.h
        Desugaring/DesugaringFamily.h
        Desugaring/ASTDesugaring.h
        Desugaring/DesugarDeclareAssign.h
        Desugaring/DesugarForStatement.h
        Desugaring/DesugarForEachStatement.h
        misc/Gensym.h
        misc/Timer.h
        Compiler.h
        Compiler.cpp
        AST/ASTVisitor/ASTOptimizations.h
        AST/ASTVisitor/ASTVariableChecking.h
        AST/ASTVisitor/ASTVariableChecking.cpp
        AST/ASTVisitor/GlobalScope/ASTGlobalScope.h
        AST/ASTVisitor/GlobalScope/ASTGlobalScope.cpp
        AST/ASTVisitor/TypeInference.h
        AST/ASTVisitor/TypeInference.cpp
        Lowering/LoweringFunctionCall.h
        AST/ASTNodes/ASTInstructions.cpp
        AST/ASTNodes/ASTInstructions.h
        AST/ASTVisitor/GlobalScope/ASTParameterPromotion.h
        AST/ASTVisitor/GlobalScope/ASTRegisterReuse.h
        AST/ASTVisitor/GlobalScope/NormalizeArrayAssign.h
        Desugaring/DesugarStruct.h
        Lowering/LoweringStruct.h
        Lowering/LoweringPointer.h
        AST/ASTVisitor/ASTLowerTypes.h
        Lowering/LoweringNil.h
        Optimization/FunctionCallHoisting.h
        Lowering/LoweringFunctionDef.h
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
)