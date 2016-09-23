#pragma once

#include "ErrorReporter.h"
#include "Logger.h"
#include "catch.hpp"

class DummyReporter : public LErrorReporter {
public:
    virtual void Write(const std::string &Text) override;


    virtual void WriteLine(const std::string &Text) override;


    virtual void Info(const std::string &Text) override;


    virtual void Warning(const std::string &Text) override;


    virtual void Error(const std::string &Text) override;


    virtual void Fatal(const std::string &Text) override;

};

namespace Leviathan{
class TestLogger : public Logger {
public:
    TestLogger(const std::string &file) : Logger(file){ }

    void Error(const std::string &data) override {

        Logger::Error(data);
        REQUIRE(false);
    }

    void Warning(const std::string &data) override {

        Logger::Warning(data);
        REQUIRE(false);
    }

    void Fatal(const std::string &Text) override {

        Logger::Fatal(Text);
        REQUIRE(false);
    }
};
}
