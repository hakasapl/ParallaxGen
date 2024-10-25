#include "ParallaxGenUI.hpp"

#include <future>

using namespace std;

void ParallaxGenUI::init() {
  wxApp::SetInstance(new wxApp());
  if (!wxEntryStart(nullptr, nullptr)) {
    throw runtime_error("Failed to initialize wxWidgets");
  }
}

void ParallaxGenUI::updateUI() { wxTheApp->Yield(); }

auto ParallaxGenUI::promptForSelection(const string &Message, const std::vector<std::wstring> &Options) -> size_t {
  if (wxIsMainThread()) {
    // If we're on the main thread, directly show the dialog
    return guiSelectionPrompt(Message, Options);
  }

  // If not on the main thread, use std::promise and wxTheApp->CallAfter
  std::promise<size_t> Promise;
  std::future<size_t> Future = Promise.get_future();

  wxTheApp->CallAfter(
      [&Promise, Message, Options]() mutable { Promise.set_value(guiSelectionPrompt(Message, Options)); });

  return Future.get(); // Wait for the result
}

auto ParallaxGenUI::guiSelectionPrompt(const string &Message, const std::vector<std::wstring> &Options) -> size_t {
  wxArrayString WxOptions;
  for (const auto &Option : Options) {
    WxOptions.Add(Option);
  }

  wxSingleChoiceDialog Dialog(nullptr, Message, "ParallaxGen", WxOptions);
  if (Dialog.ShowModal() == wxID_OK) {
    return Dialog.GetSelection();
  }

  return 0;
}
