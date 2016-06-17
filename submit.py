import time
import os
import sys
import argparse
import json
import datetime
from bonnie.submission import Submission

def print_report(results):
    for result in results:
      if 'description' in result:
        print "Description:", result['description']
      if 'traceback' in result:
        print "Traceback:"
        print result['traceback']
      if 'output' in result:
        output = result['output']
        if 'client_returncode' in output:
          print "Client Returncode:", output['client_returncode']
        if 'server_returncode' in output:
          print "Server Returncode:", output['server_returncode']
        if 'server_console' in output:
          print "Server Console:"
          print output['server_console']
        if 'client_console' in output:
          print "Client Console:"
          print output['client_console']

def main():
  parser = argparse.ArgumentParser(description='Submits code to the Udacity site.')
  parser.add_argument('quiz', choices = ['proxy_server', 'proxy_cache'])
  parser.add_argument('--provider', choices = ['gt', 'udacity'], default = 'gt')
  parser.add_argument('--environment', choices = ['local', 'development', 'staging', 'production'], default = 'production')

  args = parser.parse_args()

  path_map = { 'proxy_server': '.', 'proxy_cache': '.'}

  quiz_map = {'proxy_server': 'pr3_proxy_server', 'proxy_cache': 'pr3_proxy_cache'}

  files_map = {'pr3_proxy_server': ['handle_with_curl.c', 'webproxy.c'],
               'pr3_proxy_cache':  ['handle_with_cache.c', 'shm_channel.h', 'webproxy.c', 'shm_channel.c', 'simplecached.c']}

  quiz = quiz_map[args.quiz]

  app_data_dir = os.path.abspath(".bonnie")

  submission = Submission('cs8803-02', quiz, 
                          filenames = files_map[quiz], 
                          environment = args.environment, 
                          provider = args.provider)

  timestamp = "{:%Y-%m-%d-%H-%M-%S}".format(datetime.datetime.now())


  while not submission.poll():
    time.sleep(3.0)

  if submission.result():
    result = submission.result()

    filename = "%s-result-%s.json" % (args.quiz, timestamp)

    with open(filename, "w") as fd:
      json.dump(result, fd, indent=4, separators=(',', ': '))

    for t in result['tests']:
      description = '{:70s}'.format(t['description'][:69]+":")
      passfail = t['output']['passfail']
      print '%s %s' % (description, passfail.rjust(9))

    print "(Details available in %s.)" % os.path.join(path_map[args.quiz], filename)

  elif submission.error_report():
    error_report = submission.error_report()

    filename = "%s-error-report-%s.json" % (args.quiz, timestamp)

    with open(filename, "w") as fd:
      json.dump(error_report, fd, indent=4, separators=(',', ': '))

    print "Something went wrong.  Please see the error report in %s." % os.path.join(path_map[args.quiz], filename)

  else:
    print "Unknown error."

if __name__ == '__main__':
  main()
