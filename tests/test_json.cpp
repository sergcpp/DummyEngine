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

    {
        // Test types
        {
            // JsNumber
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

        {
            // JsString
            JsString s1("qwe123");
            std::stringstream ss;
            s1.Write(ss);
            JsString s2;
            require(s2.Read(ss));
            require(s1 == s2);
            s2 = JsString{ "asd111" };
            require(s2 == JsString{ "asd111" });
        }

        {
            // JsArray
            JsArray a1;
            a1.Push(JsNumber{ 1 });
            a1.Push(JsNumber{ 2 });
            a1.Push(JsString{ "qwe123" });
            require(a1.Size() == 3);
            require((JsNumber)a1[0] == Approx(1));
            require((JsNumber)a1[1] == Approx(2));
            require((JsString)a1[2] == JsString{ "qwe123" });
            std::stringstream ss;
            a1.Write(ss);
            JsArray a2;
            require(a2.Read(ss));
            require(a2.Size() == 3);
            require((JsNumber)a2[0] == Approx(1));
            require((JsNumber)a2[1] == Approx(2));
            require((JsString)a2[2] == JsString{ "qwe123" });

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

        {
            // JsObject
            JsObject obj;
            obj["123"] = JsNumber{ 143 };
            obj["asdf"] = JsString{ "asdfsdf" };
            obj["123"] = JsNumber{ 46 };
            require(obj.Size() == 2);
            require((JsNumber)obj["123"] == JsNumber{ 46 });
            require((JsString)obj["asdf"] == JsString{ "asdfsdf" });
            std::stringstream ss;
            obj.Write(ss);
            JsObject _obj;
            require(_obj.Read(ss));
            require(_obj.Size() == 2);
            require((JsNumber)_obj["123"] == Approx(46));
            require((JsString)_obj["asdf"] == JsString{ "asdfsdf" });

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

        {
            // JsLiteral
            JsLiteral lit(JS_TRUE);
            require(lit.val == JS_TRUE);
            std::stringstream ss;
            lit.Write(ss);

            ss.seekg(0, std::ios::beg);

            JsLiteral _lit(JS_NULL);
            require(_lit.Read(ss));
            require(_lit.val == JS_TRUE);

            // check equality
            JsLiteral lit1(JS_FALSE), lit2(JS_FALSE), lit3(JS_NULL);
            require(lit1 == lit2);
            require(lit1 != lit3);
        }

        {
            // JsElement
            JsElement _el1(16);
            const JsElement &el1 = _el1;
            require_nothrow((JsNumber)_el1);
            require_nothrow((const JsNumber &)el1);
            require_throws((JsString)_el1);
            require_throws((const JsString &)el1);
            require_throws((JsArray)_el1);
            require_throws((const JsArray &)el1);
            require_throws((JsObject)_el1);
            require_throws((const JsObject &)el1);
            require_throws((JsLiteral)_el1);
            require_throws((const JsLiteral &)el1);

            JsElement _el2("my string");
            const JsElement &el2 = _el2;
            require_nothrow((JsString)_el2);
            require_nothrow((const JsString &)el2);
            require_throws((JsNumber)_el2);
            require_throws((const JsNumber &)el2);
            require_throws((JsArray)el2);
            require_throws((const JsArray &)el2);
            require_throws((JsObject)el2);
            require_throws((const JsObject &)el2);
            require_throws((JsLiteral)el2);
            require_throws((const JsLiteral &)el2);

            JsElement _el3(JS_TYPE_ARRAY);
            const JsElement &el3 = _el3;
            require_nothrow((JsArray)_el3);
            require_nothrow((const JsArray &)el3);
            require_throws((JsNumber)_el3);
            require_throws((const JsNumber &)el3);
            require_throws((JsString)_el3);
            require_throws((const JsString &)el3);
            require_throws((JsObject)_el3);
            require_throws((const JsObject &)el3);
            require_throws((JsLiteral)_el3);
            require_throws((const JsLiteral &)el3);

            JsElement _el4(JS_TYPE_OBJECT);
            const JsElement &el4 = _el4;
            require_nothrow((JsObject)_el4);
            require_nothrow((const JsObject &)el4);
            require_throws((JsNumber)_el4);
            require_throws((const JsNumber &)el4);
            require_throws((JsString)_el4);
            require_throws((const JsString &)el4);
            require_throws((JsArray)_el4);
            require_throws((const JsArray &)el4);
            require_throws((JsLiteral)_el4);
            require_throws((const JsLiteral &)el4);

            JsElement _el5(JS_NULL);
            const JsElement &el5 = _el5;
            require_nothrow((JsLiteral)_el5);
            require_nothrow((const JsLiteral &)el5);
            require_throws((JsNumber)_el5);
            require_throws((const JsNumber &)el5);
            require_throws((JsString)_el5);
            require_throws((const JsString &)el5);
            require_throws((JsArray)_el5);
            require_throws((const JsArray &)el5);
            require_throws((JsObject)_el5);
            require_throws((const JsObject &)el5);
        }
    }

    {
        // Complex test1
        bool flag = true;
        JsElement el(JS_NULL);
        std::stringstream ss(json_example);
        require(el.Read(ss));

AGAIN1:
        auto &root = (JsObject &)el;
        require(root.Size() == 1);
        JsObject &widget = (JsObject &)root["widget"];
        require(widget.Size() == 4);
        require(widget["debug"] == JsString{ "on" });
        JsObject &window = (JsObject &)widget["window"];
        require(window.Size() == 4);
        require(window["title"] == JsString{ "Sample Konfabulator Widget" });
        require(window["name"] == JsString{ "main_window" });
        require(window["width"] == JsNumber{ 500 });
        require(window["height"] == JsNumber{ 500 });
        JsObject &image = (JsObject &)widget["image"];
        require(image.Size() == 5);
        require(image["src"] == JsString{ "Images/Sun.png" });
        require(image["name"] == JsString{ "sun1" });
        require(image["hOffset"] == JsNumber{ -250 });
        require(image["vOffset"] == JsNumber{ 250 });
        require(image["alignment"] == JsString{ "center" });
        JsObject &text = (JsObject &)widget["text"];
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

    {
        // Complex test2
        bool flag = true;
        JsElement el(JS_NULL);
        std::stringstream ss(json_example2);
        require(el.Read(ss));

AGAIN2:
        auto &root = (JsObject &)el;
        require(root.Size() == 1);
        JsObject &glossary = (JsObject &)root["glossary"];
        require(glossary.Size() == 2);
        require(glossary["title"] == JsString{ "example glossary" });
        JsObject &gloss_div = (JsObject &)glossary["GlossDiv"];
        require(gloss_div.Size() == 2);
        require(gloss_div["title"] == JsString{ "S" });
        JsObject &gloss_list = (JsObject &)gloss_div["GlossList"];
        require(gloss_list.Size() == 1);
        JsObject &gloss_entry = (JsObject &)gloss_list["GlossEntry"];
        require(gloss_entry.Size() == 7);
        require(gloss_entry["ID"] == JsString{ "SGML" });
        require(gloss_entry["SortAs"] == JsString{ "SGML" });
        require(gloss_entry["GlossTerm"] == JsString{ "Standard Generalized Markup Language" });
        require(gloss_entry["Acronym"] == JsString{ "SGML" });
        require(gloss_entry["Abbrev"] == JsString{ "ISO 8879:1986" });
        require(gloss_entry["GlossSee"] == JsString{ "markup" });
        JsObject &gloss_def = (JsObject &)gloss_entry["GlossDef"];
        require(gloss_def.Size() == 2);
        require(gloss_def["para"] == JsString{ "A meta-markup language, used to create markup languages such as DocBook." });
        JsArray &gloss_see_also = (JsArray &)gloss_def["GlossSeeAlso"];
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
        JsElement el(JS_NULL);
        std::stringstream ss(json_example3);

AGAIN3:
        require(el.Read(ss));

        auto &root = (JsObject &)el;
        require(root.Size() == 1);
        JsObject &menu = (JsObject &)root["menu"];
        require(menu.Size() == 3);
        require(menu["id"] == JsString{ "file" });
        require(menu["value"] == JsString{ "File" });
        JsObject &popup = (JsObject &)menu["popup"];
        require(popup.Size() == 1);
        JsArray &menuitem = (JsArray &)popup["menuitem"];
        require(menuitem.Size() == 3);
        auto &_0 = (JsObject &)menuitem[0];
        require(_0.Size() == 2);
        auto &_1 = (JsObject &)menuitem[1];
        require(_1.Size() == 2);
        auto &_2 = (JsObject &)menuitem[2];
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

    /*{ // Initializer lists
        JsArray arr = { JsNumber{ 0.0 }, JsNumber{ 0.0 }, JsNumber{ 1.0 }, JsNumber{ 2.0 }, JsString{ "str" }, JsArray{ JsString{ "qwe" }, JsNumber{ 4.0 } } };
        require(arr.at(0) == JsNumber{ 0.0 });
        require(arr.at(1) == JsNumber{ 0.0 });
        require(arr.at(2) == JsNumber{ 1.0 });
        require(arr.at(3) == JsNumber{ 2.0 });
        require(arr.at(4) == JsString{ "str" });
        require(((const JsArray &)arr.at(5)).at(0) == JsString{ "qwe" });
        require(((const JsArray &)arr.at(5)).at(1) == JsNumber{ 4.0 });
    }*/
}
