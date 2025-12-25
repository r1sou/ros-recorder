import argparse
import os,shutil
from datetime import datetime,date

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dataset', type=str, default='', help='dataset path')
    args = parser.parse_args()

    return args

def is_yyyy_mm_dd(s: str) -> bool:
    try:
        datetime.strptime(s, '%Y-%m-%d')
        return True
    except ValueError:
        return False

def main(args):
    today = date.today()
    for directory in os.listdir(args.dataset):
        if(os.path.isdir(os.path.join(args.dataset, directory)) and is_yyyy_mm_dd(directory)):
            diff = (today - datetime.strptime(directory, '%Y-%m-%d').date()).days
            if(diff > 1):
                shutil.rmtree(os.path.join(args.dataset, directory))

if __name__ == '__main__':
    args = parse_args()
    main(args)