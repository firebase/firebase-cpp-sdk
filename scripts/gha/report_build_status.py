# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Installing prerequisites:
#
# sudo python3 -m pip install python-dateutil progress attrs

"""A utility to report on daily build status.

USAGE:
  python3 scripts/gha/report_build_status.py \
    --token ${{github.token}}
"""

import datetime
import dateutil
import dateutil.parser
import dateutil.relativedelta
import dateutil.utils
import fcntl
import io
import os
import pickle
import progress
import progress.bar
import re
import requests
import shutil
import sys
import tempfile
import zipfile
import pickle

from absl import app
from absl import flags
from absl import logging

import firebase_github
import summarize_test_results

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "token", None,
    "github.token: A token to authenticate on your repository.")

flags.DEFINE_string(
    "start", None,
    "start date of the range to report, default = [--days] days before start")

flags.DEFINE_string(
    "end", None,
    "end date of the range to report, default = today")

flags.DEFINE_string(
    "days", '7',
    "If start date is unspecified, go this many days back")

flags.DEFINE_bool(
    "output_markdown", False,
    "Output a Markdown-formatted table.")

flags.DEFINE_bool(
    "reverse", False,
    "Reverse output, so most recent is first.")

flags.DEFINE_bool(
    "output_header", True,
    "Output a table header row. Forced true if outputting markdown.")

flags.DEFINE_bool(
    "output_username", False,
    "Include a username column in the outputted table, otherwise include a blank column in text or no column in Markdown.")

flags.DEFINE_bool(
    "include_blank_column", True,
    "In text output, include a blank column to match the build log spreadsheet format.")

flags.DEFINE_string(
    "write_cache", None,
    "Write a cache file that can be used with --read_cache on a subsequent run.")

flags.DEFINE_string(
    "read_cache", None,
    "Read a cache file that was written by a previous run via --write_cache.")

flags.DEFINE_enum(
    "report", "daily_log",
    ["daily_log", "test_summary"],
    "Choose whether to report a daily build/test log or a summary of failing and flaky tests.")

flags.DEFINE_integer(
    "summary_count", 10,
    "If --report=test_summary, how many of the top tests to show.")

flags.DEFINE_enum(
    "summary_type", "all", ["all", "errors", "flakes"],
    "Whether to include flakes, errors, or all in the test summary.")

flags.DEFINE_bool(
    "summary_include_crashes", True,
    "Whether to include CRASH/TIMEOUT in the test summary.")

flags.DEFINE_bool(
    "firestore", False,
    "Report on Firestore tests rather than on general tests.")

_WORKFLOW_TESTS = 'integration_tests.yml'
_WORKFLOW_PACKAGING = 'cpp-packaging.yml'
_TRIGGER_USER = 'firebase-workflow-trigger[bot]'
_BRANCH = 'main'
_LIMIT = 400  # Hard limit on how many jobs to fetch.

_PASS_TEXT = "Pass"
_FAILURE_TEXT = "Failure"
_FLAKY_TEXT = "Pass (flaky)"

general_test_time = ' 09:0'
firestore_test_time = ' 10:0'

def rename_key(old_dict,old_name,new_name):
    """Rename a key in a dictionary, preserving the order."""
    new_dict = {}
    for key,value in zip(old_dict.keys(),old_dict.values()):
        new_key = key if key != old_name else new_name
        new_dict[new_key] = old_dict[key]
    return new_dict


def english_list(items, sep=','):
  """Format a list in English. If there are two items, separate with "and".
     If more than 2 items, separate with commas as well.
  """
  if len(items) == 2:
    return items[0] + " and " + items[1]
  else:
    if len(items) > 2:
      items[len(items)-1] = 'and ' + items[len(items)-1]
    return (sep+' ').join(items)


def decorate_url(text, url):
  """Put the text in a URL and replace spaces with nonbreaking spaces.
     If not outputting Markdown, this does nothing.
  """
  if not FLAGS.output_markdown:
    return text
  return ("[%s](%s)" % (text.replace(" ", "&nbsp;"), url))


def analyze_log(text, url):
  """Do a simple analysis of the log summary text to determine if the build
     or test succeeded, flaked, or failed.
  """
  if not text: text = ""
  build_status = decorate_url(_PASS_TEXT, url)
  test_status = decorate_url(_PASS_TEXT, url)
  if '[BUILD] [ERROR]' in text or '[BUILD] [FAILURE]' in text:
    build_status = decorate_url(_FAILURE_TEXT, url)
  elif '[BUILD] [FLAKINESS]' in text:
    build_status =decorate_url(_FLAKY_TEXT, url)
  if '[TEST] [ERROR]' in text or '[TEST] [FAILURE]' in text:
    test_status = decorate_url(_FAILURE_TEXT, url)
  elif '[TEST] [FLAKINESS]' in text:
    test_status = decorate_url(_FLAKY_TEXT, url)
  return (build_status, test_status)


def format_errors(all_errors, severity, event):
  """Return a list of English-language formatted errors."""
  product_errors = []
  if severity not in all_errors: return None
  if event not in all_errors[severity]: return None

  errors = all_errors[severity][event]
  total_errors = 0
  individual_errors = 0
  for product, platform_dict in errors.items():
    platforms = list(platform_dict.keys())

    if product == 'missing_log':
      product_name = 'missing logs'
    elif product == 'gma':
      product_name = product.upper()
    else:
      product_name = product.replace('_', ' ').title()

    if 'iOS' in platforms:
      all_simulator = True
      for descriptors in platform_dict['iOS']['description']:
        if 'simulator_' not in descriptors:
          all_simulator = False
      if all_simulator:
        platform_dict = rename_key(platform_dict, 'iOS', 'iOS simulator')
        platforms = list(platform_dict.keys())

    if 'Android' in platforms:
      all_emulator = True
      for descriptors in platform_dict['Android']['description']:
        if 'emulator_' not in descriptors:
          all_emulator = False
      if all_emulator:
        platform_dict = rename_key(platform_dict, 'Android', 'Android emulator')
        platforms = list(platform_dict.keys())

    total_errors += 1
    individual_errors += len(platforms)
    platforms_text = english_list(platforms)
    if product == 'missing_log':
      product_errors.insert(0, '%s on %s' % (product_name, platforms_text))
    else:
      product_errors.append('%s on %s' % (product_name, platforms_text))

  event_text = event.lower()
  severity_text = 'flake' if severity == 'FLAKINESS' else severity.lower()

  if total_errors == 0:
    return None

  final_text = english_list(product_errors, ';' if ',' in ''.join(product_errors) else ',')
  if total_errors == 1:
    if 'missing logs' in final_text:
      final_text = final_text.replace('missing logs', 'missing %s logs' % event_text)
      return final_text
    else:
      final_text = 'in ' + final_text
  else:
    final_text = ('including ' if 'missing logs' in final_text else 'in ') + final_text

  final_text = '%s%s %s%s %s' % ('a ' if individual_errors == 1 else '',
                                 event_text,
                                 severity_text,
                                 's' if individual_errors > 1 else '',
                                 final_text)
  return final_text


def aggregate_errors_from_log(text, debug=False):
  if not text: return {}
  text += '\n'
  errors = {}
  lines = text.split('\n')
  current_product = None
  event = None
  severity = None
  platform = None
  other = None
  product = None

  for line in lines:
    if debug: print(line)
    if not current_product:
      m = re.search(r'^([a-z_]+):', line)
      if m:
        current_product = m.group(1)
    else:
      # Got a current product
      if len(line) == 0:
        current_product = None
      else:
        m = re.search(
          r'\[(BUILD|TEST)\] \[(ERROR|FAILURE|FLAKINESS)\] \[([a-zA-Z]+)\] (\[.*\])',
          line)
        if m:
          event = m.group(1)
          severity = m.group(2)
          if severity == "FAILURE": severity = "ERROR"
          platform = m.group(3)
          other = m.group(4)
          product = current_product

          if severity not in errors:
            errors[severity] = {}
          if event not in errors[severity]:
            errors[severity][event] = {}
          if product not in errors[severity][event]:
            errors[severity][event][product] = {}
          if platform not in errors[severity][event][product]:
            errors[severity][event][product][platform] = {}
            errors[severity][event][product][platform]['description'] = set()
            errors[severity][event][product][platform]['test_list'] = set()
          errors[severity][event][product][platform]['description'].add(other)
        else:
          m2 = re.search(r"failed tests: \[\'(.*)\'\]", line)
          if m2:
            test_list = m2.group(1).split("', '")
            for test_name in test_list:
              errors[severity][event][product][platform]['test_list'].add(test_name)
  return errors


def create_notes(text, debug=False):
  """Combine the sets of errors into a single string.
  """
  if not text: return ''
  errors = aggregate_errors_from_log(text, debug)

  log_items = []
  text = format_errors(errors, 'ERROR', 'BUILD')
  if text: log_items.append(text)
  text = format_errors(errors, 'ERROR', 'TEST')
  if text: log_items.append(text)
  text = format_errors(errors, 'FLAKINESS', 'TEST')
  if text: log_items.append(text)
  if len(log_items) == 0:
    text = format_errors(errors, 'FLAKINESS', 'BUILD')
    if text: log_items.append(text)

  if len(log_items) == 0:
    return ''
  if len(log_items) == 2 and ' and ' in ''.join(log_items):
    log_items[0] += ','
  final_text = english_list(log_items)
  final_text = final_text[0].capitalize() + final_text[1:] + '.'
  return final_text


def get_message_from_github_log(logs_zip,
                                regex_filename,
                                regex_line, debug=False):
  """Find a specific line inside a single file from a GitHub run's logs."""
  for log in logs_zip.namelist():
    if re.search(regex_filename, log):
      log_text = logs_zip.read(log).decode()
      if debug: print(log_text)
      m = re.search(regex_line, log_text, re.MULTILINE | re.DOTALL)
      if m:
        return m
  return None


def main(argv):
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")
  if not FLAGS.verbosity:
    logging.set_verbosity(logging.WARN)
  end_date = (dateutil.parser.parse(FLAGS.end) if FLAGS.end else dateutil.utils.today()).date()
  start_date = (dateutil.parser.parse(FLAGS.start) if FLAGS.start else dateutil.utils.today() - dateutil.relativedelta.relativedelta(days=int(FLAGS.days)-1)).date()
  all_days = set()
  if FLAGS.output_markdown:
    # Forced options if outputting Markdown.
    FLAGS.output_header = True
    FLAGS.output_username = False
    global _FAILURE_TEXT, _PASS_TEXT, _FLAKY_TEXT
    _FAILURE_TEXT = "❌ **" + _FAILURE_TEXT  + "**"
    _PASS_TEXT = "✅ " + _PASS_TEXT
    _FLAKY_TEXT = _PASS_TEXT + " (flaky)"

  if FLAGS.read_cache:
    logging.info("Reading cache file: %s", FLAGS.read_cache)
    with open(FLAGS.read_cache, "rb") as handle:
      fcntl.lockf(handle, fcntl.LOCK_SH)  # For reading, shared lock is OK.
      _cache = pickle.load(handle)
      fcntl.lockf(handle, fcntl.LOCK_UN)

      all_days = _cache['all_days']
      source_tests = _cache['source_tests']
      packaging_runs = _cache['packaging_runs']
      package_tests = _cache['package_tests']
  else:
    _cache = {}

    with progress.bar.Bar('Reading jobs...', max=3) as bar:
      workflow_id = _WORKFLOW_TESTS
      all_runs = firebase_github.list_workflow_runs(FLAGS.token, workflow_id, _BRANCH, 'schedule', _LIMIT)
      bar.next()
      source_tests = {}
      for run in reversed(all_runs):
        run['date'] = dateutil.parser.parse(run['created_at'], ignoretz=True)
        run['day'] = run['date'].date()
        day = str(run['date'].date())
        if day in source_tests: continue
        if run['status'] != 'completed': continue
        if run['day'] < start_date or run['day'] > end_date: continue
        run['duration'] = dateutil.parser.parse(run['updated_at'], ignoretz=True) - run['date']
        compare_test_time = firestore_test_time if FLAGS.firestore else general_test_time
        if compare_test_time in str(run['date']):
          source_tests[day] = run
          all_days.add(day)

      workflow_id = _WORKFLOW_PACKAGING
      all_runs = firebase_github.list_workflow_runs(FLAGS.token, workflow_id, _BRANCH, 'schedule', _LIMIT)
      bar.next()
      packaging_runs = {}
      packaging_run_ids = set()
      for run in reversed(all_runs):
        run['date'] = dateutil.parser.parse(run['created_at'], ignoretz=True)
        day = str(run['date'].date())
        run['day'] = run['date'].date()
        if day in packaging_runs: continue
        if run['status'] != 'completed': continue
        if run['day'] < start_date or run['day'] > end_date: continue
        day = str(run['date'].date())
        all_days.add(day)
        packaging_runs[day] = run
        packaging_run_ids.add(str(run['id']))

      workflow_id = _WORKFLOW_TESTS
      all_runs = firebase_github.list_workflow_runs(FLAGS.token, workflow_id, _BRANCH, 'workflow_dispatch', _LIMIT)
      bar.next()
      package_tests_all = []
      for run in reversed(all_runs):
        run['date'] = dateutil.parser.parse(run['created_at'], ignoretz=True)
        day = str(run['date'].date())
        run['day'] = run['date'].date()
        if day not in packaging_runs: continue
        if run['status'] != 'completed': continue
        if run['day'] < start_date or run['day'] > end_date: continue
        if run['triggering_actor']['login'] != _TRIGGER_USER: continue
        package_tests_all.append(run)

    # For each workflow_trigger run of the tests, determine which packaging run it goes with.
    package_tests = {}

    logging.info("Source tests: %s %s", list(source_tests.keys()),  [source_tests[r]['id'] for r in source_tests.keys()])
    logging.info("Packaging runs: %s %s", list(packaging_runs.keys()), [packaging_runs[r]['id'] for r in packaging_runs.keys()])

    with progress.bar.Bar('Downloading triggered workflow logs...', max=len(package_tests_all)) as bar:
      for run in package_tests_all:
        day = str(run['date'].date())
        if day in package_tests:
          # Packaging triggers two tests. For Firestore, we want the larger run ID (the second run triggered).
          if FLAGS.firestore and int(package_tests[day]['id']) > int(run['id']):
            bar.next()
            continue
          # For general tests we want the smaller run ID (the first run triggered).
          if not FLAGS.firestore and int(package_tests[day]['id']) < int(run['id']):
            bar.next()
            continue

        packaging_run = 0

        logs_url = run['logs_url']
        headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': 'Bearer %s' % FLAGS.token}
        with requests.get(logs_url, headers=headers, stream=True) as response:
          if response.status_code == 200:
            logs_compressed_data = io.BytesIO(response.content)
            logs_zip = zipfile.ZipFile(logs_compressed_data)
            m = get_message_from_github_log(
              logs_zip,
              r'check_and_prepare/.*Run if.*expanded.*then.*\.txt',
              r'\[warning\]Downloading SDK package from previous run:[^\n]*/([0-9]*)$')
            if m:
              packaging_run = m.group(1)
        if str(packaging_run) in packaging_run_ids:
          package_tests[day] = run
        bar.next()

    logging.info("Package tests: %s %s", list(package_tests.keys()), [package_tests[r]['id'] for r in package_tests.keys()])

    with progress.bar.Bar('Downloading test summaries...', max=len(source_tests)+len(package_tests)) as bar:
      for tests in source_tests, package_tests:
        for day in tests:
          run = tests[day]
          run['log_success'] = True
          run['log_results'] = ''
          artifacts = firebase_github.list_artifacts(FLAGS.token, run['id'])
          found_artifacts = False
          # There are possibly multiple artifacts, so iterate through all of them,
          # and extract the relevant ones into a temp folder, and then summarize them all.
          with tempfile.TemporaryDirectory() as tmpdir:
            for a in artifacts:
              if 'log-artifact' in a['name']:
                print("Checking this artifact:", a['name'], "\n")
                artifact_contents = firebase_github.download_artifact(FLAGS.token, a['id'])
                if artifact_contents:
                  found_artifacts = True
                  artifact_data = io.BytesIO(artifact_contents)
                  artifact_zip = zipfile.ZipFile(artifact_data)
                  artifact_zip.extractall(path=tmpdir)
            if found_artifacts:
              (success, results) = summarize_test_results.summarize_logs(tmpdir, False, False, True)
              print("Results:", success, "    ", results, "\n")
              run['log_success'] = success
              run['log_results'] = results

          if not found_artifacts:
            # Artifacts expire after some time, so if they are gone, we need
            # to read the GitHub logs instead.  This is much slower, so we
            # prefer to read artifacts instead whenever possible.
            logging.info("Reading github logs for run %s instead", run['id'])

            logs_url = run['logs_url']
            headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': 'Bearer %s' % FLAGS.token}
            with requests.get(logs_url, headers=headers, stream=True) as response:
              if response.status_code == 200:
                logs_compressed_data = io.BytesIO(response.content)
                logs_zip = zipfile.ZipFile(logs_compressed_data)
                m = get_message_from_github_log(
                  logs_zip,
                  r'summarize-results/.*Summarize results into GitHub',
                  r'\[error\]INTEGRATION TEST FAILURES\n—+\n(.*)$')
                if m:
                  run['log_success'] = False
                  m2 = re.match(r'(.*?)^' + day, m.group(1), re.MULTILINE | re.DOTALL)
                  if m2:
                    run['log_results'] = m2.group(1)
                  else:
                    run['log_results'] = m.group(1)
                  logging.debug("Integration test results: %s", run['log_results'])

          tests[day] = run
          bar.next()

    _cache['all_days'] = all_days
    _cache['source_tests'] = source_tests
    _cache['packaging_runs'] = packaging_runs
    _cache['package_tests'] = package_tests

    if FLAGS.write_cache:
      logging.info("Writing cache file: %s", FLAGS.write_cache)
      with open(FLAGS.write_cache, "wb") as handle:
        fcntl.lockf(handle, fcntl.LOCK_EX)  # For writing, need exclusive lock.
        pickle.dump(_cache, handle, protocol=pickle.HIGHEST_PROTOCOL)
        fcntl.lockf(handle, fcntl.LOCK_UN)

  prev_notes = ''
  last_good_day = None

  output = ""

  if FLAGS.output_markdown:
    output += "### Testing History (last %d days)\n\n" % len(all_days)

  table_fields = (
      ["Date"] +
      (["Username"] if FLAGS.output_username else ([] if FLAGS.output_markdown else [""])) +
      ([""] if FLAGS.include_blank_column and not FLAGS.output_markdown else []) +
      ["Build vs Source Repo", "Test vs Source Repo",
       "SDK Packaging", "Build vs SDK Package", "Test vs SDK Package",
       "Notes"]
  )
  if FLAGS.output_markdown:
      row_prefix = "| "
      row_separator = "|"
      row_suffix = " |"
  else:
      row_prefix = row_suffix = ""
      row_separator = "\t"

  table_header_string = row_prefix + row_separator.join(table_fields) + row_suffix
  table_row_fmt = row_prefix + row_separator.join(["%s" for f in table_fields]) + row_suffix

  if FLAGS.output_header:
    output += table_header_string + "\n"
    if FLAGS.output_markdown:
      output += table_row_fmt.replace("%s", "---").replace(" ", "") + "\n"

  days_sorted = sorted(all_days)
  if FLAGS.reverse: days_sorted = reversed(days_sorted)
  for day in days_sorted:
    day_str = day
    if FLAGS.output_markdown:
        day_str = day_str.replace("-", "&#8209;")  # non-breaking hyphen.
    if day not in package_tests or day not in packaging_runs or day not in source_tests:
      day = last_good_day
    if not day: continue
    last_good_day = day
    source_tests_log = analyze_log(source_tests[day]['log_results'], source_tests[day]['html_url'])
    if packaging_runs[day]['conclusion'] == "success":
      package_build_log = _PASS_TEXT
    else:
      package_build_log = _FAILURE_TEXT
    package_build_log = decorate_url(package_build_log, packaging_runs[day]['html_url'])
    package_tests_log = analyze_log(package_tests[day]['log_results'], package_tests[day]['html_url'])

    notes = create_notes(source_tests[day]['log_results'] if source_tests[day]['log_results'] else package_tests[day]['log_results'])
    if FLAGS.output_markdown and notes:
        notes = "<details><summary>&nbsp;</summary>" + notes + "</details>"
    if notes == prev_notes and not FLAGS.output_markdown:
      if len(notes) > 0: notes = "'''"  # Creates a "ditto" mark.
    else:
      prev_notes = notes

    table_row_contents = (
        [day_str] +
        ([os.getlogin()] if FLAGS.output_username else ([] if FLAGS.output_markdown else [""])) +
        ([""] if FLAGS.include_blank_column and not FLAGS.output_markdown else []) +
        [source_tests_log[0],
         source_tests_log[1],
         package_build_log,
         package_tests_log[0],
         package_tests_log[1],
         notes]
    )
    output += (table_row_fmt % tuple(table_row_contents)) + "\n"

  if FLAGS.report == "daily_log":
    print(output)
  elif FLAGS.report == "test_summary":
    test_list = {}
    for day in days_sorted:
      if day in source_tests and source_tests[day]['log_results']:
        errors = aggregate_errors_from_log(source_tests[day]['log_results'])
        test_link = source_tests[day]['html_url']
      elif day in package_tests and package_tests[day]['log_results']:
        errors = aggregate_errors_from_log(package_tests[day]['log_results'])
        test_link = package_tests[day]['html_url']
      else:
        continue

      sev_list = []
      if FLAGS.summary_type == "all" or FLAGS.summary_type == "flakes":
        sev_list.append('FLAKINESS')
      if FLAGS.summary_type == "all" or FLAGS.summary_type == "errors":
        sev_list.append('ERROR')
      for sev in sev_list:
        if sev in errors and 'TEST' in errors[sev]:
          test_entries = errors[sev]['TEST']
          for product, platform_dict in test_entries.items():
            if product == "missing_log":
              continue
            platforms = list(platform_dict.keys())
            for platform in platforms:
              test_names = list(test_entries[product][platform]['test_list'])
              if not test_names:
                test_names = ['Unspecified test']
              for test_name in test_names:
                if test_name == "CRASH/TIMEOUT":
                    if not FLAGS.summary_include_crashes: continue
                    else: test_name = "Crash or timeout"
                test_id = "%s | %s | %s | %s" % (sev.lower(), product, platform, test_name)
                if test_id not in test_list:
                  test_list[test_id] = {}
                  test_list[test_id]['count'] = 0
                  test_list[test_id]['links'] = []
                test_list[test_id]['count'] += 1
                test_list[test_id]['links'].append(test_link)
                test_list[test_id]['latest'] = day

    test_list_sorted = reversed(sorted(test_list.keys(), key=lambda x: test_list[x]['count']))
    if FLAGS.output_header:
      if FLAGS.output_markdown:
        print("| # | Latest | Product | Platform | Test Info |")
        print("|---|---|---|---|---|")
      else:
        print("Count\tLatest\tSeverity\tProduct\tPlatform\tTest Name")

    num_shown = 0

    for test_id in test_list_sorted:
      (severity, product, platform, test_name) = test_id.split(" | ")
      days_ago = (dateutil.utils.today() - dateutil.parser.parse(test_list[test_id]['latest'])).days
      if days_ago <= 0:
        latest = "Today"
      else:
        latest = "%s day%s ago" % (days_ago, '' if days_ago == 1 else 's')
      if FLAGS.output_markdown:
        if severity == "error":
          severity = "(failure)"
        elif severity == "flakiness":
          severity = "(flaky)"
        latest = latest.replace(" ", "&nbsp;")
        product = product.replace("_", " ")
        product = product.upper() if product == "gma" else product.title()
        if len(test_list[test_id]['links']) > 0:
          latest = "[%s](%s)" % (latest, test_list[test_id]['links'][-1])

        link_list = []
        seen = set()
        num = 1

        for link in test_list[test_id]['links']:
          if link not in seen:
            seen.add(link)
            link_list.append("[%d](%s)" % (num, link))
            num += 1
        # If test_name looks like FirebaseSomethingTest.Something, link it to code search.
        m = re.match(r"(Firebase[A-Za-z]*Test)\.(.*)", test_name)
        if m:
          search_url = "http://github.com/search?q=repo:firebase/firebase-cpp-sdk%%20\"%s,%%20%s\"" % (m.group(1), m.group(2))
          test_name_str = "[%s](%s)" % (test_name, search_url)
        else:
          test_name_str = test_name

        print("| %d | %s | %s | %s | %s&nbsp;%s<br/>&nbsp;&nbsp;&nbsp;Logs: %s |" % (
            test_list[test_id]['count'], latest,
            product, platform,
            test_name_str, severity, " ".join(link_list)))
      else:
        print("%d\t%s\t%s\t%s\t%s\t%s" % (test_list[test_id]['count'], latest, severity, product, platform, test_name))
      num_shown += 1
      if num_shown >= FLAGS.summary_count:
        break


if __name__ == "__main__":
  flags.mark_flag_as_required("token")
  app.run(main)
