import QtQuick
import QtTest
import Omaweb

// The content blocker the harness runs refreshes its lists from the fixtures
// in `tests/ui/filter-lists`, so no UI suite opens a connection off the
// machine (#614). The blocker is the real one: the check fetches, compiles and
// swaps the lists in exactly as it does for a reader.
TestCase {
    id: testCase
    name: "ContentBlockerLists"
    when: true

    function settled() {
        return !contentBlocker.compiling && contentBlocker.subscriptions.every(function (list) {
            return list.updateStatus === "current";
        });
    }

    function test_everyListIsServedFromTheMachine() {
        verify(contentBlocker.subscriptions.length > 0, "the harness subscribes to no list");
        for (const list of contentBlocker.subscriptions) {
            const address = Qt.resolvedUrl(list.updateAddress);
            verify(address.toString().startsWith("file:///"), list.id + " is fetched from "
                   + list.updateAddress);
        }
    }

    function test_theListCheckFetchesCompilesAndSwapsTheFixtures() {
        contentBlocker.updateAllSubscriptions();
        tryVerify(settled, 30000, "the lists did not all become current");
        compare(contentBlocker.compilationReport.invalidRuleCount, 0);
        verify(contentBlocker.compilationReport.acceptedRuleCount > 0);
        verify(contentBlocker.cosmeticStyleSheet("https://fixture-easylist.example/").indexOf(
                   ".fixture-easylist-banner") >= 0, "the first fixture's rule is not in force");
        verify(contentBlocker.cosmeticStyleSheet("https://fixture-easyprivacy.example/").indexOf(
                   ".fixture-easyprivacy-beacon") >= 0,
               "the second fixture's rule is not in force");
    }
}
