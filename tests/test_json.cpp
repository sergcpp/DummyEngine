#include "test_common.h"

#include <sstream>

#include "../Json.h"

namespace {
const char json_example[] = "{"
                            "\t\"widget\": {\n"
                            "\t\t\"debug\": \"on\",\n"
                            "\t\t\"window\": {\n"
                            "\t\t\t\"title\": \"Sample Konfabulator Widget\",\n"
                            "\t\t\t\"name\": \"main_window\",\n"
                            "\t\t\t\"width\": 500,\n"
                            "\t\t\t\"height\": 500\n"
                            "\t\t},\n"
                            "\t\t\"image\": { \n"
                            "\t\t\t\"src\": \"Images/Sun.png\",\n"
                            "\t\t\t\"name\": \"sun1\",\n"
                            "\t\t\t\"hOffset\": -250,\n"
                            "\t\t\t\"vOffset\": 250,\n"
                            "\t\t\t\"alignment\": \"center\"\n"
                            "\t\t},\n"
                            "\t\t\"text\": {\n"
                            "\t\t\t\"data\": \"Click Here\",\n"
                            "\t\t\t\"size\": 36,\n"
                            "\t\t\t\"style\": \"bold\",\n"
                            "\t\t\t\"name\": \"text1\",\n"
                            "\t\t\t\"hOffset\": 250,\n"
                            "\t\t\t\"vOffset\": 100,\n"
                            "\t\t\t\"alignment\": \"center\",\n"
                            "\t\t\t\"onMouseUp\": \"sun1.opacity = (sun1.opacity / 100) * 90;\"\n"
                            "\t\t}\n"
                            "\t}"
                            "}";

const char json_example2[] = "{\n"
                             "\t\"glossary\": {\n"
                             "\t\t\"title\": \"example glossary\",\n"
                             "\t\t\"GlossDiv\": {\n"
                             "\t\t\t\"title\": \"S\",\n"
                             "\t\t\t\"GlossList\": {\n"
                             "\t\t\t\t\"GlossEntry\": {\n"
                             "\t\t\t\t\t\"ID\": \"SGML\",\n"
                             "\t\t\t\t\t\"SortAs\": \"SGML\",\n"
                             "\t\t\t\t\t\"GlossTerm\": \"Standard Generalized Markup Language\",\n"
                             "\t\t\t\t\t\"Acronym\": \"SGML\",\n"
                             "\t\t\t\t\t\"Abbrev\": \"ISO 8879:1986\",\n"
                             "\t\t\t\t\t\"GlossDef\": {\n"
                             "\t\t\t\t\t\t\"para\": \"A meta-markup language, used to create markup languages such as DocBook.\",\n"
                             "\t\t\t\t\t\t\"GlossSeeAlso\": [\"GML\", \"XML\"]\n"
                             "\t\t\t\t\t},\n"
                             "\t\t\t\t\t\"GlossSee\": \"markup\"\n"
                             "\t\t\t\t}\n"
                             "\t\t\t}\n"
                             "\t\t}\n"
                             "\t}\n"
                             "}";

const char json_example3[] = "{\n"
                             "\t\"menu\": {\n"
                             "\t\t\"id\": \"file\",\n"
                             "\t\t\"value\": \"File\",\n"
                             "\t\t\"popup\": {\n"
                             "\t\t\t\"menuitem\": [\n"
                             "\t\t\t\t{\"value\": \"New\", \"onclick\": \"CreateNewDoc()\"},\n"
                             "\t\t\t\t{\"value\": \"Open\", \"onclick\": \"OpenDoc()\"},\n"
                             "\t\t\t\t{\"value\": \"Close\", \"onclick\": \"CloseDoc()\"}\n"
                             "\t\t\t]\n"
                             "\t\t}\n"
                             "\t}\n"
                             "}";
}

void test_json() {

    { // Test types
        { // JsNumber
            JsNumber n1(3.1568);
            std::stringstream ss;
            n1.Write(ss);
            JsNumber n2;
            require(n2.Read(ss));
            require(n2 == Approx(n1));
            n1 = JsNumber{ 5 };
            n2 = JsNumber{ 5 };
            require(n2 == double(5));
            require(n1 == n2);
        }

        { // JsString
            JsString s1("qwe123");
            std::stringstream ss;
            s1.Write(ss);
            JsString s2;
            require(s2.Read(ss));
            require(s1 == s2);
            s2 = JsString{ "asd111" };
            require(s2 == JsString{ "asd111" });
        }

        { // JsArray
            JsArray a1;
            a1.Push(JsNumber{ 1 });
            a1.Push(JsNumber{ 2 });
            a1.Push(JsString{ "qwe123" });
            require(a1.Size() == 3);
            require(a1[0].as_num() == Approx(1));
            require(a1[1].as_num() == Approx(2));
            require(a1[2].as_str() == JsString{ "qwe123" });
            std::stringstream ss;
            a1.Write(ss);
            JsArray a2;
            require(a2.Read(ss));
            require(a2.Size() == 3);
            require(a2[0].as_num() == Approx(1));
            require(a2[1].as_num() == Approx(2));
            require(a2[2].as_str() == JsString{ "qwe123" });

            require_throws(a2.at(3));

            // check equality
            JsArray a3, a4, a5;
            a3.Push(JsString{ "asdf" });
            a3.Push(JsString{ "zxc" });
            a4.Push(JsString{ "asdf" });
            a4.Push(JsString{ "zxc" });
            a5.Push(JsString{ "asdf" });
            a5.Push(JsString{ "zxc1" });
            require(a3 == a4);
            require(a3 != a5);
        }

        { // JsObject
            JsObject obj;
            obj["123"] = JsNumber{ 143 };
            obj["asdf"] = JsString{ "asdfsdf" };
            obj["123"] = JsNumber{ 46 };
            require(obj.Size() == 2);
            require(obj["123"].as_num() == JsNumber{ 46 });
            require(obj["asdf"].as_str() == JsString{ "asdfsdf" });
            std::stringstream ss;
            obj.Write(ss);
            JsObject _obj;
            require(_obj.Read(ss));
            require(_obj.Size() == 2);
            require(_obj["123"].as_num() == Approx(46));
            require(_obj["asdf"].as_str() == JsString{ "asdfsdf" });

            require_throws(_obj.at("non exists"));

            // check equality
            JsObject obj1, obj2, obj3;
            obj1["123"] = JsNumber{ 143 };
            obj1["asdf"] = JsString{ "asdfsdf" };
            obj2["123"] = JsNumber{ 143 };
            obj2["asdf"] = JsString{ "asdfsdf" };
            obj3["123"] = JsNumber{ 143 };
            obj3["asdf"] = JsString{ "asdfsdf1" };
            require(obj1 == obj2);
            require(obj1 != obj3);
        }

        { // JsLiteral
            JsLiteral lit(JsLiteralType::True);
            require(lit.val == JsLiteralType::True);
            std::stringstream ss;
            lit.Write(ss);

            ss.seekg(0, std::ios::beg);

            JsLiteral _lit(JsLiteralType::Null);
            require(_lit.Read(ss));
            require(_lit.val == JsLiteralType::True);

            // check equality
            JsLiteral lit1(JsLiteralType::False), lit2(JsLiteralType::False), lit3(JsLiteralType::Null);
            require(lit1 == lit2);
            require(lit1 != lit3);
        }

        { // JsElement
            JsElement _el1(16);
            const JsElement &el1 = _el1;
            require_nothrow(_el1.as_num());
            require_nothrow(el1.as_num());
            require_throws(_el1.as_str());
            require_throws(el1.as_str());
            require_throws(_el1.as_arr());
            require_throws(el1.as_arr());
            require_throws(_el1.as_obj());
            require_throws(el1.as_obj());
            require_throws(_el1.as_lit());
            require_throws(el1.as_lit());

            JsElement _el2("my string");
            const JsElement &el2 = _el2;
            require_nothrow(_el2.as_str());
            require_nothrow(el2.as_str());
            require_throws(_el2.as_num());
            require_throws(el2.as_num());
            require_throws(_el2.as_arr());
            require_throws(el2.as_arr());
            require_throws(_el2.as_obj());
            require_throws(el2.as_obj());
            require_throws(_el2.as_lit());
            require_throws(el2.as_lit());

            JsElement _el3(JsType::Array);
            const JsElement &el3 = _el3;
            require_nothrow(_el3.as_arr());
            require_nothrow(el3.as_arr());
            require_throws(_el3.as_num());
            require_throws(el3.as_num());
            require_throws(_el3.as_str());
            require_throws(el3.as_str());
            require_throws(_el3.as_obj());
            require_throws(el3.as_obj());
            require_throws(_el3.as_lit());
            require_throws(el3.as_lit());

            JsElement _el4(JsType::Object);
            const JsElement &el4 = _el4;
            require_nothrow(_el4.as_obj());
            require_nothrow(el4.as_obj());
            require_throws(_el4.as_num());
            require_throws(el4.as_num());
            require_throws(_el4.as_str());
            require_throws(el4.as_str());
            require_throws(_el4.as_arr());
            require_throws(el4.as_arr());
            require_throws(_el4.as_lit());
            require_throws(el4.as_lit());

            JsElement _el5(JsLiteralType::Null);
            const JsElement &el5 = _el5;
            require_nothrow(_el5.as_lit());
            require_nothrow(el5.as_lit());
            require_throws(_el5.as_num());
            require_throws(el5.as_num());
            require_throws(_el5.as_str());
            require_throws(el5.as_str());
            require_throws(_el5.as_arr());
            require_throws(el5.as_arr());
            require_throws(_el5.as_obj());
            require_throws(el5.as_obj());
        }
    }

    { // Complex test1
        bool flag = true;
        JsElement el(JsLiteralType::Null);
        std::stringstream ss(json_example);
        require(el.Read(ss));

AGAIN1:
        JsObject &root = el.as_obj();
        require(root.Size() == 1);
        JsObject &widget = root["widget"].as_obj();
        require(widget.Size() == 4);
        require(widget["debug"] == JsString{ "on" });
        JsObject &window = widget["window"].as_obj();
        require(window.Size() == 4);
        require(window["title"] == JsString{ "Sample Konfabulator Widget" });
        require(window["name"] == JsString{ "main_window" });
        require(window["width"] == JsNumber{ 500 });
        require(window["height"] == JsNumber{ 500 });
        JsObject &image = widget["image"].as_obj();
        require(image.Size() == 5);
        require(image["src"] == JsString{ "Images/Sun.png" });
        require(image["name"] == JsString{ "sun1" });
        require(image["hOffset"] == JsNumber{ -250 });
        require(image["vOffset"] == JsNumber{ 250 });
        require(image["alignment"] == JsString{ "center" });
        JsObject &text = widget["text"].as_obj();
        require(text.Size() == 8);
        require(text["data"] == JsString{ "Click Here" });
        require(text["size"] == JsNumber{ 36 });
        require(text["style"] == JsString{ "bold" });
        require(text["name"] == JsString{ "text1" });
        require(text["hOffset"] == JsNumber{ 250 });
        require(text["vOffset"] == JsNumber{ 100 });
        require(text["alignment"] == JsString{ "center" });
        require(text["onMouseUp"] == JsString{ "sun1.opacity = (sun1.opacity / 100) * 90;" });

        if (flag) {
            flag = false;
            ss.clear();
            ss.seekg(0);
            root.Write(ss);
            goto AGAIN1;
        }
    }

    { // Complex test2
        bool flag = true;
        JsElement el(JsLiteralType::Null);
        std::stringstream ss(json_example2);
        require(el.Read(ss));

AGAIN2:
        JsObject &root = el.as_obj();
        require(root.Size() == 1);
        JsObject &glossary = root["glossary"].as_obj();
        require(glossary.Size() == 2);
        require(glossary["title"] == JsString{ "example glossary" });
        JsObject &gloss_div = glossary["GlossDiv"].as_obj();
        require(gloss_div.Size() == 2);
        require(gloss_div["title"] == JsString{ "S" });
        JsObject &gloss_list = gloss_div["GlossList"].as_obj();
        require(gloss_list.Size() == 1);
        JsObject &gloss_entry = gloss_list["GlossEntry"].as_obj();
        require(gloss_entry.Size() == 7);
        require(gloss_entry["ID"] == JsString{ "SGML" });
        require(gloss_entry["SortAs"] == JsString{ "SGML" });
        require(gloss_entry["GlossTerm"] == JsString{ "Standard Generalized Markup Language" });
        require(gloss_entry["Acronym"] == JsString{ "SGML" });
        require(gloss_entry["Abbrev"] == JsString{ "ISO 8879:1986" });
        require(gloss_entry["GlossSee"] == JsString{ "markup" });
        JsObject &gloss_def = gloss_entry["GlossDef"].as_obj();
        require(gloss_def.Size() == 2);
        require(gloss_def["para"] == JsString{ "A meta-markup language, used to create markup languages such as DocBook." });
        JsArray &gloss_see_also = gloss_def["GlossSeeAlso"].as_arr();
        require(gloss_see_also.Size() == 2);
        require(gloss_see_also[0] == JsString{ "GML" });
        require(gloss_see_also[1] == JsString{ "XML" });

        if (flag) {
            flag = false;
            ss.clear();
            ss.seekg(0);
            root.Write(ss);
            goto AGAIN2;
        }
    }

    {   // Complex test3
        bool flag = true;
        JsElement el(JsLiteralType::Null);
        std::stringstream ss(json_example3);

AGAIN3:
        require(el.Read(ss));

        JsObject &root = el.as_obj();
        require(root.Size() == 1);
        JsObject &menu = root["menu"].as_obj();
        require(menu.Size() == 3);
        require(menu["id"] == JsString{ "file" });
        require(menu["value"] == JsString{ "File" });
        JsObject &popup = menu["popup"].as_obj();
        require(popup.Size() == 1);
        JsArray &menuitem = popup["menuitem"].as_arr();
        require(menuitem.Size() == 3);
        JsObject &_0 = menuitem[0].as_obj();
        require(_0.Size() == 2);
        JsObject &_1 = menuitem[1].as_obj();
        require(_1.Size() == 2);
        JsObject &_2 = menuitem[2].as_obj();
        require(_2.Size() == 2);
        require(_0["value"] == JsString{ "New" });
        require(_0["onclick"] == JsString{ "CreateNewDoc()" });
        require(_1["value"] == JsString{ "Open" });
        require(_1["onclick"] == JsString{ "OpenDoc()" });
        require(_2["value"] == JsString{ "Close" });
        require(_2["onclick"] == JsString{ "CloseDoc()" });

        if (flag) {
            flag = false;
            ss.clear();
            ss.seekg(0);
            root.Write(ss);
            std::string str = ss.str();
            goto AGAIN3;
        }
    }
}
