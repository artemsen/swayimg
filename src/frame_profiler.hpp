// SPDX-License-Identifier: MIT
// Frame profiler for performance investigation
// Logs frame timings and statistics for gallery lag analysis

#pragma once

#include <chrono>
#include <fstream>
#include <vector>
#include <cstdint>

class FrameProfiler {
public:
    static FrameProfiler& instance()
    {
        static FrameProfiler prof;
        return prof;
    }

    // Call at start of frame rendering
    void frame_start()
    {
        frame_count_++;
        frame_start_time_ = std::chrono::high_resolution_clock::now();
    }

    // Call at end of frame (after commit_surface)
    void frame_end()
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto frame_ms = std::chrono::duration<double, std::milli>(end_time - frame_start_time_).count();

        frame_times_.push_back(frame_ms);

        // Log every frame to CSV file for analysis
        if (frame_count_ == 1) {
            // Open file on first frame
            log_file_.open("frame_profile.csv");
            log_file_ << "frame_num,frame_time_ms\n";
        }

        if (log_file_.is_open()) {
            log_file_ << frame_count_ << "," << frame_ms << "\n";
            if (frame_count_ % 100 == 0) {
                log_file_.flush(); // Flush every 100 frames
            }
        }

        // Simulate gallery navigation for profiling
        // After collecting enough frames, trigger navigation events
        if (should_trigger_navigation()) {
            trigger_navigation_event();
        }

        // Stop profiling after collecting enough frames
        if (frame_count_ >= 2000) {
            stop_profiling_flag_ = true;
        }
    }

    // Check if we should stop profiling
    bool should_stop_profiling() const
    {
        return stop_profiling_flag_;
    }

    // For use by application to detect profiling mode
    bool is_profiling_enabled() const
    {
        return profiling_enabled_;
    }

    // Get statistics
    void print_stats()
    {
        if (frame_times_.empty()) {
            return;
        }

        double sum = 0;
        double min = frame_times_[0];
        double max = frame_times_[0];

        for (auto t : frame_times_) {
            sum += t;
            if (t < min) min = t;
            if (t > max) max = t;
        }

        double avg = sum / frame_times_.size();
        double fps = 1000.0 / avg;

        printf("\n");
        printf("╔════════════════════════════════════════╗\n");
        printf("║   FRAME PROFILING RESULTS              ║\n");
        printf("╚════════════════════════════════════════╝\n");
        printf("\n");
        printf("  Frames captured: %zu\n", frame_times_.size());
        printf("  Average frame time: %.2f ms\n", avg);
        printf("  Average FPS: %.1f\n", fps);
        printf("  Min frame time: %.2f ms\n", min);
        printf("  Max frame time: %.2f ms\n", max);
        printf("\n");
        printf("  Performance targets:\n");
        printf("    - 60 FPS target: 16.67 ms/frame\n");
        printf("    - 30 FPS target: 33.33 ms/frame\n");
        printf("\n");

        // Count frames meeting targets
        int count_60fps = 0;
        int count_30fps = 0;
        for (auto t : frame_times_) {
            if (t < 16.67) count_60fps++;
            if (t < 33.33) count_30fps++;
        }

        double pct_60 = (100.0 * count_60fps / frame_times_.size());
        double pct_30 = (100.0 * count_30fps / frame_times_.size());

        printf("  Frames meeting 60 FPS: %.1f%% (%d/%zu)\n", pct_60, count_60fps, frame_times_.size());
        printf("  Frames meeting 30 FPS: %.1f%% (%d/%zu)\n", pct_30, count_30fps, frame_times_.size());
        printf("\n");

        // Analysis
        if (pct_60 >= 70) {
            printf("  ✓ GOOD: Most frames meet 60 FPS target\n");
        } else if (pct_60 >= 50) {
            printf("  ⚠ FAIR: About half of frames meet 60 FPS target\n");
        } else if (pct_30 >= 70) {
            printf("  ⚠ POOR: Frames mostly meeting 30 FPS target\n");
        } else {
            printf("  ✗ BAD: Most frames below 30 FPS target\n");
        }
        printf("\n");

        printf("  Detailed log saved to: frame_profile.csv\n");
        printf("\n");

        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    // Callback for navigation events (called by application when navigating)
    void on_navigation_event()
    {
        navigation_event_count_++;
    }

private:
    FrameProfiler()
    {
        // Check if profiling is enabled via environment variable
        const char* prof_env = std::getenv("SWAYIMG_PROFILE");
        profiling_enabled_ = (prof_env != nullptr);
    }

    bool should_trigger_navigation() const
    {
        // Trigger navigation every ~60 frames (roughly every second at 60 FPS)
        return profiling_enabled_ && (frame_count_ % 60 == 0 && frame_count_ < 1800);
    }

    void trigger_navigation_event()
    {
        // This will be called to signal the application to perform gallery navigation
        // The application will handle the actual navigation in its event loop
        navigation_event_count_++;
    }

    std::chrono::high_resolution_clock::time_point frame_start_time_;
    std::vector<double> frame_times_;
    std::ofstream log_file_;
    size_t frame_count_ = 0;
    size_t navigation_event_count_ = 0;
    bool stop_profiling_flag_ = false;
    bool profiling_enabled_ = false;
};
