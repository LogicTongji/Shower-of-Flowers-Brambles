#pragma once

#include <filesystem>
#include <string_view>

void SetGuiDiagnosticsRoot(const std::filesystem::path& root);
void ResetGuiDiagnostics();
void WriteGuiDiagnostic(std::string_view message);
