#include "test_common.h"

#include <sstream>

#include "../Preprocessor.h"

void test_preprocessor() {
    printf("Test preprocessor       | ");

    auto HasMacro = [](const glslx::Preprocessor &preprocessor, const std::string &macro) -> bool {
        const auto &macros = preprocessor.macros();

        auto it = std::find_if(macros.cbegin(), macros.cend(), [&macro](auto &&entry) { return entry.name == macro; });

        return it != macros.cend();
    };

    { // no macros
        static const char source[] = "#version 450\n"
                                     "#extension GL_GOOGLE_include_directive : require\n"
                                     "void main/* this is a comment*/(/*void*/) {\n"
                                     "    return/*   */ 42;\n"
                                     "}";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == source);
        require(preprocessor.error().empty());
    }
    { // no macros (2)
        static const char source[] = "1.0001 1.00001f vec4(1.0f, 0.2, 0.223, 1.0001f);";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == source);
        require(preprocessor.error().empty());
    }
    { // no macros (3)
        static const char source[] = "float c = nebula(layer2_coord * 3.0) * 0.35 - 0.05";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == source);
        require(preprocessor.error().empty());
    }
    { // no macros (4)
        static const char source[] = R"(
		void main() {
			printf("test \n"); 
		})";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == source);
        require(preprocessor.error().empty());
    }
    { // no macros (5)
        static const char source[] = R"(
		Line above

		// "\p"
		Line below
		float getNumber() {
			return 1.0;
		})";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == source);
        require(preprocessor.error().empty());
    }
    { // no macros (6)
        static const char source[] = "A;// Commentary";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == source);
        require(preprocessor.error().empty());
    }
    { // object-like macro
        static const char source[] = "#define VALUE 42\n"
                                     "#define VALUE 42\n"
                                     "void main() {\n"
                                     "    return VALUE;\n"
                                     "}";
        static const char expected[] = "void main() {\n"
                                       "    return 42;\n"
                                       "}";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // object-like macro (2)
        static const char source[] = "#   define Foo";
        static const char expected[] = "";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
        require(preprocessor.macros().size() == 1);
        require(HasMacro(preprocessor, "Foo"));
    }
    { // default-initialized macro
        static const char source[] = "#define VALUE\n"
                                     "VALUE";
        static const char expected[] = "";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // line directive
        static const char source[] = "__LINE__\n"
                                     "__LINE__\n"
                                     "__LINE__";
        static const char expected[] = "1\n2\n3";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // function-like macro
        static const char source[] = "#define ADD(X, Y) X + Y\n"
                                     "#define ADD(X, Y) X + Y\n"
                                     "void main() {\n"
                                     "    return ADD(2, 3);\n"
                                     "}";
        static const char expected[] = "void main() {\n"
                                       "    return 2 + 3;\n"
                                       "}";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // function-like macro (2)
        static const char source[] = "#define FOO(X, Y) Foo.getValue(X, Y)\n"
                                     "FOO(42, input.value)";
        static const char expected[] = "Foo.getValue(42, input.value)";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // function-like macro (3)
        static const char source[] = "#define FOO(X) \\\nint X; \\\nint X ## _Additional;\nFOO(Test)";
        static const char expected[] = "int Test;int Test_Additional;";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // function-like macro (4)
        static const char source[] = "#define FOO(X, Y) X(Y)\n"
                                     "FOO(Foo, Test(0, 0))";
        static const char expected[] = "Foo(Test(0, 0))";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // function-like macro (5)
        static const char source[] = "#define FOO(X)\n"
                                     "FOO(42)";
        static const char expected[] = "";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // function-like macro (6)
        static const char source[] = "#define unpack_unorm_16(x) (float(x) / 65535.0)\n"
                                     "unpack_unorm_16((42) + 1)";
        static const char expected[] = "(float((42) + 1) / 65535.0)";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // function-like macro (7)
        static const char source[] = "#define ADD(X, Y) X + Y\n"
                                     "void main() {\n"
                                     "    return ADD(2,\n"
                                     "               3);\n"
                                     "}";
        static const char expected[] = "void main() {\n"
                                       "    return 2 + 3;\n"
                                       "}";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // function-like macro (8)
        static const char source[] = "#define ADD(X, Y) X + Y\n"
                                     "void main() {\n"
                                     "    return ADD(ADD(2, 1), 3);\n"
                                     "}";
        static const char expected[] = "void main() {\n"
                                       "    return 2 + 1 + 3;\n"
                                       "}";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // stringify
        static const char source[] = "#define FOO(Name) #Name\n"
                                     " FOO(Text)";
        static const char expected[] = " Text";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // concatenate
        static const char source[] = "AAA   ## BB";
        static const char expected[] = "AAABB";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // include directive
        static const char input[] = "#include <system>\n"
                                    "two";
        static const char system_input[] = "one\n";
        static const char expected[] = "one\n"
                                       "two";
        glslx::preprocessor_config_t config;
        config.include_callback = [](const char *path, const bool is_system_path) {
            require(is_system_path);
            require(strcmp(path, "system") == 0);
            return std::make_unique<std::istringstream>(system_input);
        };
        glslx::Preprocessor preprocessor(input, config);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // system/non-system include directives
        static const char source[] = "#include <system>\n"
                                     "#include \"non_system_path\"\n"
                                     "void main() {\n"
                                     "    return ADD(2, 3);\n"
                                     "}";
        static const char expected[] = "void main() {\n"
                                       "    return ADD(2, 3);\n"
                                       "}";
        int include_count = 0;
        glslx::preprocessor_config_t config;
        config.include_callback = [&include_count](const char *path, const bool is_system_path) {
            if (include_count == 0) {
                require(is_system_path);
                require(strcmp(path, "system") == 0);
            } else if (include_count == 1) {
                require(!is_system_path);
                require(strcmp(path, "non_system_path") == 0);
            }
            ++include_count;
            return std::make_unique<std::istringstream>();
        };
        glslx::Preprocessor preprocessor(source, config);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
        require(include_count == 2);
    }
    { // include directive without newline
        static const char source[] = "#include <iostream>";
        static const char expected[] = "";
        glslx::preprocessor_config_t config;
        config.include_callback = [](const char *path, const bool is_system_path) {
            require(is_system_path);
            require(strcmp(path, "iostream") == 0);
            return std::make_unique<std::istringstream>();
        };
        glslx::Preprocessor preprocessor(source, config);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // simple #if #endif
        static const char source[] = "#if FOO\none#endif\n two three";
        static const char expected[] = "\n two three";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // simple #if #else #endif
        static const char source[] = "#if FOO\n"
                                     " // this block will be skiped\n"
                                     " if block\n"
                                     "#else\n"
                                     " else block #endif";
        static const char expected[] = "\n else block ";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // simple #if #else #endif (2)
        static const char source[] = "#if 1\n"
                                     " if block\n"
                                     "#else\n"
                                     " else block #endif";
        static const char expected[] = " if block\n";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // simple #if #elif #else #endif
        static const char source[] = "#if 0\none\n#elif 1\ntwo\n#else\nthree\n#endif";
        static const char expected[] = "two\n";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // simple #if #elif #else #endif (2)
        static const char source[] = "#if( 0 )\none\n#elif( 1 )\ntwo\n#else\nthree\n#endif";
        static const char expected[] = "two\n";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // more complex #elif case
        static const char source[] = "#if 0\n"
                                     "    one\n"
                                     "#elif 0\n"
                                     "    two\n"
                                     "#elif 1\n"
                                     "    three\n"
                                     "#else\n"
                                     "    four\n"
                                     "#endif";
        static const char expected[] = "    three\n";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // invalid else
        static const char source[] = "#if 0\n"
                                     "    one\n"
                                     "#elif 0\n"
                                     "    two\n"
                                     "#else\n"
                                     "    four\n"
                                     "#elif 1\n"
                                     "    three\n"
                                     "#endif";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process().empty());
        require(!preprocessor.error().empty());
    }
    { // nested conditions
        static const char source[] = "#if 1\n"
                                     "    one\n"
                                     "#if 0\n"
                                     "    two\n"
                                     "#endif\n"
                                     "    four\n"
                                     "#elif 0\n"
                                     "    three\n"
                                     "#endif";
        static const char expected[] = "    one\n"
                                       "\n"
                                       "    four\n";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // #ifdef block
        static const char source[] = "#ifdef FOO\n"
                                     "    one\n"
                                     "#endif\n"
                                     "    two";
        static const char expected[] = "\n"
                                       "    two";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // #ifdef (2)
        static const char source[] = "#define FOO\n"
                                     "#ifdef FOO\n"
                                     "    one\n"
                                     "#endif";
        static const char expected[] = "    one\n";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // #ifndef block
        static const char source[] = "#ifndef FOO\n"
                                     "    one\n"
                                     "#endif\n"
                                     "    two";
        static const char expected[] = "    one\n"
                                       "\n"
                                       "    two";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // #ifndef + #undef
        static const char source[] = "#define FOO\n"
                                     "#undef FOO\n"
                                     "#ifndef FOO\n"
                                     "    one\n"
                                     "#endif\n"
                                     "#undef FOO";
        static const char expected[] = "    one\n\n";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // include directive with guards
        static const char source_main[] = R"(
			#define FOO
			
			#include "source1.h"
            #include "source2.h"

			#ifndef FILE_H
			#define FILE_H

			#ifdef FOO
				#define BAR(x) x
			#endif

			#ifdef FOO2
				#define BAR(x) x,x
			#endif

			#endif
		)";

        static const char source1[] = R"(
			#ifndef SOURCE1_H
			#define SOURCE1_H

			#include <system>

			#endif
		)";

        static const char source2[] = R"(
			#ifndef SOURCE2_H
			#define SOURCE2_H

			#include "source3.h"

			#endif
		)";

        static const char source3[] = R"(
			#ifndef SOURCE3_H
			#define SOURCE3_H

			#include <system>

			#endif
		)";

        static const char system_source[] = R"(
			#ifndef SYSTEM_H
			#define SYSTEM_H

            #define DEFINE1

            #ifdef DEFINE1
                #define FOO3			
			    int x = 42;
            #elif defined(UNDEFINED)
                #define FOO3
                int x = 14;
            #else
			    #define FOO3			
			    int x = 16;
            #endif

			#endif
		)";

        static const char expected[] = R"(
						
			
						
			
						
            
                            			    int x = 42;
            

			
		
			
		            
						
			
						
			
			
		
			
		
			
		
						
										

			

			
		)";
        glslx::preprocessor_config_t config;
        config.include_callback = [](const char *path, const bool is_system_path) {
            if (strcmp(path, "source1.h") == 0) {
                require(!is_system_path);
                return std::make_unique<std::istringstream>(source1);
            } else if (strcmp(path, "source2.h") == 0) {
                require(!is_system_path);
                return std::make_unique<std::istringstream>(source2);
            } else if (strcmp(path, "source3.h") == 0) {
                require(!is_system_path);
                return std::make_unique<std::istringstream>(source3);
            } else {
                require(is_system_path);
                require(strcmp(path, "system") == 0);
                return std::make_unique<std::istringstream>(system_source);
            }
        };
        glslx::Preprocessor preprocessor(source_main, config);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // expressions
        static const char source[] = R"(
			#define A 1
			#define C 0
			#define FOO(X, Y) (X && Y)
			
			#if A && B
				#define PASSED_0
			#else
				#define FAILED_0
			#endif

			#if A || B
				#define PASSED_1
			#else
				#define FAILED_1
			#endif

			#if !A
				#define PASSED_2
			#else
				#define FAILED_2
			#endif

			#if A + B
				#define PASSED_3
			#else
				#define FAILED_3
			#endif

			#if A - B
				#define PASSED_4
			#else
				#define FAILED_4
			#endif

			#if A * B
				#define PASSED_5
			#else
				#define FAILED_5
			#endif

			#if A / B
				#define PASSED_6
			#else
				#define FAILED_6
			#endif

			#if C
				#define PASSED_7
			#else
				#define FAILED_7
			#endif
)";
        glslx::Preprocessor preprocessor(source);
        preprocessor.Process();
        require(preprocessor.error().empty());

        require(!HasMacro(preprocessor, "PASSED_0"));
        require(HasMacro(preprocessor, "FAILED_0"));

        require(HasMacro(preprocessor, "PASSED_1"));
        require(!HasMacro(preprocessor, "FAILED_1"));

        require(!HasMacro(preprocessor, "PASSED_2"));
        require(HasMacro(preprocessor, "FAILED_2"));

        require(HasMacro(preprocessor, "PASSED_3"));
        require(!HasMacro(preprocessor, "FAILED_3"));

        require(HasMacro(preprocessor, "PASSED_4"));
        require(!HasMacro(preprocessor, "FAILED_4"));

        require(!HasMacro(preprocessor, "PASSED_5"));
        require(HasMacro(preprocessor, "FAILED_5"));

        require(!HasMacro(preprocessor, "PASSED_6"));
        require(HasMacro(preprocessor, "FAILED_6"));

        require(!HasMacro(preprocessor, "PASSED_7"));
        require(HasMacro(preprocessor, "FAILED_7"));
    }
    { // expressions (2)
        static const char source[] = R"(
			#define A 1
			#define AND(X, Y) (X && Y)
			
			#if AND(A, 0)
				#define PASSED
			#else
				#define FAILED
			#endif

			#if AND(A, 1)
				#define PASSED_1
			#else
				#define FAILED_1
			#endif
)";
        glslx::Preprocessor preprocessor(source);
        preprocessor.Process();
        require(preprocessor.error().empty());

        require(!HasMacro(preprocessor, "PASSED"));
        require(HasMacro(preprocessor, "FAILED"));

        require(HasMacro(preprocessor, "PASSED_1"));
        require(!HasMacro(preprocessor, "FAILED_1"));
    }
    { // expressions (3)
        static const char source[] = R"(
#define A 1
#define C 0

#if defined(B) || defined(A)
	#define PASSED_0
#else
	#define FAILED_0
#endif

#if defined(C) && defined(A)
	#define PASSED_1
#else
	#define FAILED_2
#endif
)";
        glslx::Preprocessor preprocessor(source);
        preprocessor.Process();
        require(preprocessor.error().empty());

        require(HasMacro(preprocessor, "PASSED_0"));
        require(!HasMacro(preprocessor, "FAILED_0"));

        require(HasMacro(preprocessor, "PASSED_1"));
        require(!HasMacro(preprocessor, "FAILED_1"));
    }
    { // ???
        static const char source[] = R"(
#ifndef FOO_H
#define FOO_H

/*int foo() {
	return 0 ;//* 42; // this //* sequence can be considered as commentary's beginning
}
*/
#endif


)";
        static const char expected[] = R"(

/*int foo() {
	return 0 ;//* 42; // this //* sequence can be considered as commentary's beginning
}
*/



)";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // strip comments
        static const char source[] = R"(
int main(int argc, char** argv) {
	// TEST COMMENT
	return -1;
}
)";
        static const char expected[] = R"(
int main(int argc, char** argv) {
	
	return -1;
}
)";
        glslx::preprocessor_config_t config;
        config.strip_comments = true;
        glslx::Preprocessor preprocessor(source, config);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }
    { // extension macros
        static const char source[] = "#version 450\n"
                                     "#extension GL_KHR_shader_subgroup_basic : enable\n"
                                     "#if defined(GL_KHR_shader_subgroup_basic)\n"
                                     "#define NUM 42\n"
                                     "#else\n"
                                     "#define NUM 24\n"
                                     "#endif\n"
                                     "int func() {\n"
                                     "    return NUM;\n"
                                     "}";
        static const char expected[] = "#version 450\n"
                                       "#extension GL_KHR_shader_subgroup_basic : enable\n"
                                       "\n"
                                       "int func() {\n"
                                       "    return 42;\n"
                                       "}";
        glslx::Preprocessor preprocessor(source);
        require(preprocessor.Process() == expected);
        require(preprocessor.error().empty());
    }

    printf("OK\n");
}