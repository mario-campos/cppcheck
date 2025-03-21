/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2022 Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "testsuite.h"

#include "color.h"
#include "options.h"
#include "redirect.h"

#include <cstdio>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

std::ostringstream errout;
std::ostringstream output;

/**
 * TestRegistry
 **/
namespace {
    struct CompareFixtures {
        bool operator()(const TestFixture* lhs, const TestFixture* rhs) const {
            return lhs->classname < rhs->classname;
        }
    };
}
using TestSet = std::set<TestFixture*, CompareFixtures>;
class TestRegistry {
    TestSet _tests;
public:

    static TestRegistry &theInstance() {
        static TestRegistry testreg;
        return testreg;
    }

    void addTest(TestFixture *t) {
        _tests.insert(t);
    }

    const TestSet &tests() const {
        return _tests;
    }
};




/**
 * TestFixture
 **/

std::ostringstream TestFixture::errmsg;
unsigned int TestFixture::countTests;

std::size_t TestFixture::fails_counter = 0;
std::size_t TestFixture::todos_counter = 0;
std::size_t TestFixture::succeeded_todos_counter = 0;
std::set<std::string> TestFixture::missingLibs;

TestFixture::TestFixture(const char * const _name)
    : mVerbose(false),
    exename(),
    quiet_tests(false),
    classname(_name)
{
    TestRegistry::theInstance().addTest(this);
}


bool TestFixture::prepareTest(const char testname[])
{
    mVerbose = false;
    mTemplateFormat.clear();
    mTemplateLocation.clear();

    // Check if tests should be executed
    if (testToRun.empty() || testToRun == testname) {
        // Tests will be executed - prepare them
        mTestname = testname;
        ++countTests;
        if (quiet_tests) {
            std::putchar('.'); // Use putchar to write through redirection of std::cout/cerr
            std::fflush(stdout);
        } else {
            std::cout << classname << "::" << testname << std::endl;
        }
        return true;
    }
    return false;
}

std::string TestFixture::getLocationStr(const char * const filename, const unsigned int linenr) const
{
    return std::string(filename) + ':' + std::to_string(linenr) + '(' + classname + "::" + mTestname + ')';
}

static std::string writestr(const std::string &str, bool gccStyle = false)
{
    std::ostringstream ostr;
    if (gccStyle)
        ostr << '\"';
    for (std::string::const_iterator i = str.cbegin(); i != str.cend(); ++i) {
        if (*i == '\n') {
            ostr << "\\n";
            if ((i+1) != str.end() && !gccStyle)
                ostr << std::endl;
        } else if (*i == '\t')
            ostr << "\\t";
        else if (*i == '\"')
            ostr << "\\\"";
        else if (std::isprint(static_cast<unsigned char>(*i)))
            ostr << *i;
        else
            ostr << "\\x" << std::hex << short{*i};
    }
    if (!str.empty() && !gccStyle)
        ostr << std::endl;
    else if (gccStyle)
        ostr << '\"';
    return ostr.str();
}

bool TestFixture::assert_(const char * const filename, const unsigned int linenr, const bool condition) const
{
    if (!condition) {
        ++fails_counter;
        errmsg << getLocationStr(filename, linenr) << ": Assertion failed." << std::endl << "_____" << std::endl;
    }
    return condition;
}

void TestFixture::assertEqualsFailed(const char* const filename, const unsigned int linenr, const std::string& expected, const std::string& actual, const std::string& msg) const
{
    ++fails_counter;
    errmsg << getLocationStr(filename, linenr) << ": Assertion failed. " << std::endl
           << "Expected: " << std::endl
           << writestr(expected) << std::endl
           << "Actual: " << std::endl
           << writestr(actual) << std::endl;
    if (!msg.empty())
        errmsg << "Hint:" << std::endl << msg << std::endl;
    errmsg << "_____" << std::endl;
}

bool TestFixture::assertEquals(const char * const filename, const unsigned int linenr, const std::string &expected, const std::string &actual, const std::string &msg) const
{
    if (expected != actual) {
        assertEqualsFailed(filename, linenr, expected, actual, msg);
    }
    return expected == actual;
}

std::string TestFixture::deleteLineNumber(const std::string &message)
{
    std::string result(message);
    // delete line number in "...:NUMBER:..."
    std::string::size_type pos = 0;
    while ((pos = result.find(':', pos)) != std::string::npos) {
        // get number
        if (pos + 1 == result.find_first_of("0123456789", pos + 1)) {
            const std::string::size_type after = result.find_first_not_of("0123456789", pos + 1);
            if (after != std::string::npos
                && result.at(after) == ':') {
                // erase NUMBER
                result.erase(pos + 1, after - pos - 1);
                pos = after;
            } else {
                ++pos;
            }
        } else {
            ++pos;
        }
    }
    return result;
}

void TestFixture::assertEqualsWithoutLineNumbers(const char * const filename, const unsigned int linenr, const std::string &expected, const std::string &actual, const std::string &msg) const
{
    assertEquals(filename, linenr, deleteLineNumber(expected), deleteLineNumber(actual), msg);
}

bool TestFixture::assertEquals(const char * const filename, const unsigned int linenr, const char expected[], const std::string& actual, const std::string &msg) const
{
    return assertEquals(filename, linenr, std::string(expected), actual, msg);
}
bool TestFixture::assertEquals(const char * const filename, const unsigned int linenr, const char expected[], const char actual[], const std::string &msg) const
{
    return assertEquals(filename, linenr, std::string(expected), std::string(actual), msg);
}
bool TestFixture::assertEquals(const char * const filename, const unsigned int linenr, const std::string& expected, const char actual[], const std::string &msg) const
{
    return assertEquals(filename, linenr, expected, std::string(actual), msg);
}

bool TestFixture::assertEquals(const char * const filename, const unsigned int linenr, const long long expected, const long long actual, const std::string &msg) const
{
    if (expected != actual) {
        std::ostringstream ostr1;
        ostr1 << expected;
        std::ostringstream ostr2;
        ostr2 << actual;
        assertEquals(filename, linenr, ostr1.str(), ostr2.str(), msg);
    }
    return expected == actual;
}

void TestFixture::assertEqualsDouble(const char * const filename, const unsigned int linenr, const double expected, const double actual, const double tolerance, const std::string &msg) const
{
    if (expected < (actual - tolerance) || expected > (actual + tolerance)) {
        std::ostringstream ostr1;
        ostr1 << expected;
        std::ostringstream ostr2;
        ostr2 << actual;
        assertEquals(filename, linenr, ostr1.str(), ostr2.str(), msg);
    }
}

void TestFixture::todoAssertEquals(const char * const filename, const unsigned int linenr,
                                   const std::string &wanted,
                                   const std::string &current,
                                   const std::string &actual) const
{
    if (wanted == actual) {
        errmsg << getLocationStr(filename, linenr) << ": Assertion succeeded unexpectedly. "
               << "Result: " << writestr(wanted, true)  << std::endl << "_____" << std::endl;

        ++succeeded_todos_counter;
    } else {
        assertEquals(filename, linenr, current, actual);
        ++todos_counter;
    }
}

void TestFixture::todoAssertEquals(const char* const filename, const unsigned int linenr,
                                   const char wanted[],
                                   const char current[],
                                   const std::string& actual) const
{
    todoAssertEquals(filename, linenr, std::string(wanted), std::string(current), actual);
}


void TestFixture::todoAssertEquals(const char * const filename, const unsigned int linenr, const long long wanted, const long long current, const long long actual) const
{
    std::ostringstream wantedStr, currentStr, actualStr;
    wantedStr << wanted;
    currentStr << current;
    actualStr << actual;
    todoAssertEquals(filename, linenr, wantedStr.str(), currentStr.str(), actualStr.str());
}

void TestFixture::assertThrow(const char * const filename, const unsigned int linenr) const
{
    ++fails_counter;
    errmsg << getLocationStr(filename, linenr) << ": Assertion succeeded. "
           << "The expected exception was thrown" << std::endl << "_____" << std::endl;

}

void TestFixture::assertThrowFail(const char * const filename, const unsigned int linenr) const
{
    ++fails_counter;
    errmsg << getLocationStr(filename, linenr) << ": Assertion failed. "
           << "The expected exception was not thrown"  << std::endl << "_____" << std::endl;

}

void TestFixture::assertNoThrowFail(const char * const filename, const unsigned int linenr) const
{
    ++fails_counter;
    errmsg << getLocationStr(filename, linenr) << ": Assertion failed. "
           << "Unexpected exception was thrown"  << std::endl << "_____" << std::endl;

}

void TestFixture::complainMissingLib(const char * const libname)
{
    missingLibs.insert(libname);
}

void TestFixture::printHelp()
{
    std::cout << "Testrunner - run Cppcheck tests\n"
        "\n"
        "Syntax:\n"
        "    testrunner [OPTIONS] [TestClass::TestCase...]\n"
        "    run all test cases:\n"
        "        testrunner\n"
        "    run all test cases in TestClass:\n"
        "        testrunner TestClass\n"
        "    run TestClass::TestCase:\n"
        "        testrunner TestClass::TestCase\n"
        "    run all test cases in TestClass1 and TestClass2::TestCase:\n"
        "        testrunner TestClass1 TestClass2::TestCase\n"
        "\n"
        "Options:\n"
        "    -q                   Do not print the test cases that have run.\n"
        "    -h, --help           Print this help.\n";
}

void TestFixture::run(const std::string &str)
{
    testToRun = str;
    if (quiet_tests) {
        std::cout << '\n' << classname << ':';
        REDIRECT;
        run();
    } else
        run();
}

void TestFixture::processOptions(const options& args)
{
    quiet_tests = args.quiet();
    exename = args.exe();
}

std::size_t TestFixture::runTests(const options& args)
{
    countTests = 0;
    errmsg.str("");

    for (std::string classname : args.which_test()) {
        std::string testname;
        if (classname.find("::") != std::string::npos) {
            testname = classname.substr(classname.find("::") + 2);
            classname.erase(classname.find("::"));
        }

        for (TestFixture * test : TestRegistry::theInstance().tests()) {
            if (classname.empty() || test->classname == classname) {
                test->processOptions(args);
                test->run(testname);
            }
        }
    }

    std::cout << "\n\nTesting Complete\nNumber of tests: " << countTests << std::endl;
    std::cout << "Number of todos: " << todos_counter;
    if (succeeded_todos_counter > 0)
        std::cout << " (" << succeeded_todos_counter << " succeeded)";
    std::cout << std::endl;
    // calling flush here, to do all output before the error messages (in case the output is buffered)
    std::cout.flush();

    std::cerr << "Tests failed: " << fails_counter << std::endl << std::endl;
    std::cerr << errmsg.str();

    if (!missingLibs.empty()) {
        std::cerr << "Missing libraries: ";
        for (const std::string & missingLib : missingLibs)
            std::cerr << missingLib << "  ";
        std::cerr << std::endl << std::endl;
    }
    std::cerr.flush();
    return fails_counter;
}

void TestFixture::reportOut(const std::string & outmsg, Color /*c*/)
{
    output << outmsg << std::endl;
}

void TestFixture::reportErr(const ErrorMessage &msg)
{
    const std::string errormessage(msg.toString(mVerbose, mTemplateFormat, mTemplateLocation));
    if (errout.str().find(errormessage) == std::string::npos)
        errout << errormessage << std::endl;
}
