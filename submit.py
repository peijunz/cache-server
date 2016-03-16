import time
import os
import sys
import argparse
import json
import yaml
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
  parser.add_argument('--provider', choices = ['gt', 'udacity'], default = 'udacity')
  parser.add_argument('--environment', choices = ['local', 'development', 'staging', 'production'], default = 'development')

  args = parser.parse_args()
  
  files_map = {'proxy_server': ['handle_with_curl.c', 'webproxy.c']
               'proxy_cache':  ['handle_with_cache.c', 'shm_channel.h', 'webproxy.c', 'shm_channel.c', 'simplecached.c']}

  app_data_dir = os.path.abspath(".bonnie")

  submission = Submission('cs8803-02', quiz, 
                          filenames = files_map[args.quiz], 
                          exclude = False, 
                          environment = args.environment, 
                          provider = args.provider,
                          app_data_dir = app_data_dir)

  while not submission.poll():
    time.sleep(3.0)

  if submission.result():
    result = json.loads(submission.result())
    if len(result['errors']) > 0:
      print "Errors:"
      print_report(result['errors'])
    if len(result['failures']) > 0:      
      print "Failures:"
      print_report(result['failures'])    
    if len(result['failures']) + len(result['errors']) == 0:
      print "Correct!"
  elif submission.error_report():
    print submission.error_report()
  else:
    print "Unknown error."

if __name__ == '__main__':
  main()
