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
  // Use a promise/future to block the calling thread until the selection is made on the main thread
  promise<size_t> Promise;
  future<size_t> Future = Promise.get_future();

  // CallAfter schedules the GUI code to run on the main thread
  wxTheApp->CallAfter([&Promise, Message, Options]() mutable {
    wxArrayString WxOptions;
    for (const auto &Option : Options) {
      WxOptions.Add(Option);
    }

    wxSingleChoiceDialog Dialog(nullptr, Message, "ParallaxGen", WxOptions);
    if (Dialog.ShowModal() == wxID_OK) {
      // Set the result (selected index) on the promise when selection is made
      Promise.set_value(Dialog.GetSelection());
    } else {
      // Set a default value (or handle cancellation appropriately)
      Promise.set_value(0);
    }
  });

  // Return the selected index (or default value if canceled)
  return Future.get();
}
