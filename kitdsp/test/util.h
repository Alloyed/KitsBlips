#pragma once

#include <AudioFile.h>
#include <gtest/gtest.h>
#include <fstream>

namespace kitdsp::test {
template <typename... TColumns>
class CsvFile {
    using Row = std::tuple<TColumns...>;

   public:
    bool load(const std::string& filepath) {
        // TODO
        return false;
    }
    bool save(const std::string& filepath) {
        std::ofstream out(filepath);
        // TODO: failure checking
        write(out);
        out.close();
        return true;
    }
    void print() {
        write(std::cout);
        std::cout.flush();
    }
    std::vector<std::string> columns;
    std::vector<Row> rows;

   private:
    void write(std::ostream& out) const {
        bool comma = false;
        for (const auto& column : columns) {
            if (comma) {
                out << ",";
            }
            comma = true;
            out << column;
        }
        out << "\n";
        for (const auto& row : rows) {
            writeRow(out, row);
            out << "\n";
        }
    }

    template <std::size_t... I>
    void writeRowImpl(std::ostream& file, const Row& row, std::index_sequence<I...>) const {
        ((file << std::get<I>(row) << (I < sizeof...(I) - 1 ? "," : "")), ...);
    }

    void writeRow(std::ostream& file, const Row& row) const {
        writeRowImpl(file, row, std::index_sequence_for<TColumns...>{});
    }
};

namespace {
template <typename T>
void compareElem(const T& expected, const T& actual) {
    if constexpr (std::is_floating_point_v<T>) {
        EXPECT_NEAR(expected, actual, 1e-5);
    } else {
        EXPECT_EQ(expected, actual);
    }
}
template <typename Tuple1, typename Tuple2, std::size_t... I>
void compareTupleImpl(const Tuple1& t1, const Tuple2& t2, std::index_sequence<I...>) {
    (compareElem(std::get<I>(t1), std::get<I>(t2)), ...);
}
template <typename... Ts>
void compareTuple(const std::tuple<Ts...>& expected, const std::tuple<Ts...>& actual) {
    compareTupleImpl(expected, actual, std::index_sequence_for<Ts...>{});
}
}  // namespace

template <typename... TColumns>
inline void Snapshot(CsvFile<TColumns...>& f, std::string_view postfix = "") {
    // time to snapshot
    bool shouldUpdate = getenv("UPDATE_SNAPSHOTS") != nullptr;

    const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream ss;
    ss << PROJECT_DIR << "/test/snapshots/" << test_info->test_suite_name() << "_" << test_info->name() << postfix
       << ".csv";

    std::string filepath{ss.str()};
    ASSERT_TRUE(f.save(filepath)) << "File saving failed";

    // TODO: csv loading
    // CsvFile<TColumns...> expected;
    // bool loaded = expected.load(filepath);
    // if (!loaded || shouldUpdate) {
    //    // file may not exist, try to write
    //    ASSERT_TRUE(f.save(filepath)) << "File saving failed";
    //    return;
    //}

    // for (size_t i = 0; i < f.columns.size(); ++i) {
    //     EXPECT_STREQ(f.columns[i], expected.columns[i]);
    // }

    // for (size_t i = 0; i < f.rows.size(); ++i) {
    //     compareElem(f.rows[i], expected.rows[i]);
    // }
}

inline void Snapshot(AudioFile<float>& f, std::string_view postfix = "") {
    // pass 1: sanity check
    float SR = f.getSampleRate();
    for (size_t i = 0; i < f.getNumSamplesPerChannel(); ++i) {
        for (size_t c = 0; c < f.getNumChannels(); ++c) {
            float s = f.samples[c][i];
            ASSERT_EQ(s, s) << "NaN sample at " << i << " (" << (i / SR) << " seconds)";
            ASSERT_GE(s, -1.0f) << "clipped sample at " << i << " (" << (i / SR) << " seconds)";
            ASSERT_LE(s, 1.0f) << "clipped sample at " << i << " (" << (i / SR) << " seconds)";
        }
    }

    // time to snapshot
    bool shouldUpdate = getenv("UPDATE_SNAPSHOTS") != nullptr;

    const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream ss;
    ss << PROJECT_DIR << "/test/snapshots/" << test_info->test_suite_name() << "_" << test_info->name() << postfix
       << ".wav";

    std::string filepath{ss.str()};
    AudioFile<float> expected;
    bool loaded = expected.load(filepath);
    if (!loaded || shouldUpdate) {
        // file may not exist, try to write
        ASSERT_TRUE(f.save(filepath)) << "File saving failed";
        return;
    }
    EXPECT_EQ(f.getSampleRate(), expected.getSampleRate());
    EXPECT_EQ(f.getNumChannels(), expected.getNumChannels());
    EXPECT_DOUBLE_EQ(f.getLengthInSeconds(), f.getLengthInSeconds());

    if (f.getNumChannels() != expected.getNumChannels() ||
        f.getNumSamplesPerChannel() != expected.getNumSamplesPerChannel() ||
        f.getSampleRate() != expected.getSampleRate()) {
        return;
    }

    for (size_t i = 0; i < f.getNumSamplesPerChannel(); ++i) {
        for (size_t c = 0; c < f.getNumChannels(); ++c) {
            ASSERT_NEAR(f.samples[c][i], expected.samples[c][i], 0.01)
                << "Non-matching sample at " << i << " (" << (i / SR) << " seconds)\n"
                << "Set the environment variable UPDATE_SNAPSHOTS to update anyways";
        }
    }
}

}  // namespace kitdsp::test