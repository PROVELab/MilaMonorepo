import os
import shutil
import sys
from typing import Any, Callable

# Global flags for interactive generation
_copy_all = False
_skip_all = False
_overwrite_all = False

def interactive_file_gen(
    output_path: str,
    description: str,
    generation_func: Callable[..., Any],
    *gen_args: Any,
) -> str | None:
    """
    Handles interactive generation for a single file or directory.
    Prompts user to copy, overwrite, or skip if the path exists.
    Returns the path that was generated, or None if skipped.
    """
    global _copy_all, _skip_all, _overwrite_all

    rel_path = os.path.relpath(output_path)
    
    if os.path.exists(output_path):
        action = None
        # Check for 'all' flags to bypass prompt
        if _skip_all:
            action = 's'
        elif _copy_all:
            action = 'c'
        elif _overwrite_all:
            action = 'o'

        if not action: # Prompt user if no 'all' flag is active
            if not sys.stdin.isatty():
                action = 'o'
                print(f"Path {rel_path} already exists. No interactive stdin available, defaulting to overwrite for {description}.")

        if not action:
            while True:
                print(f"\nPath {rel_path} already exists. Press 'h' or 'help' for help")
                response = input(f"For {description}, do you want to make copy, overwrite, skip, copy all, skip all, or overwrite all (c/o/s/ca/sa/oa)? : ").strip().lower()
                
                if response in ('s', 'skip'):
                    action = 's'
                    break
                elif response in ('sa', 'skip all'):
                    _skip_all = True
                    action = 's'
                    break
                elif response in ('c', 'copy'):
                    action = 'c'
                    break
                elif response in ('ca', 'copy all'):
                    _copy_all = True
                    action = 'c'
                    break
                elif response in ('o', 'overwrite'):
                    action = 'o'
                    break
                elif response in ('oa', 'overwrite all'):
                    _overwrite_all = True
                    action = 'o'
                    break
                elif response in ('h', 'help'):
                    print("\n'copy' (c): Make a new file/directory with a unique name, like 'path_Copy1'.")
                    print("'overwrite' (o): Delete the existing path and generate a new one.")
                    print("'skip' (s): Do nothing and keep the existing path.")
                    print("'copy all' (ca): Apply 'copy' to this and all subsequent items without asking.")
                    print("'skip all' (sa): Apply 'skip' to this and all subsequent items without asking.")
                    print("'overwrite all' (oa): Apply 'overwrite' to this and all subsequent items without asking.\n")
                else:
                    print("Invalid response, try again.")

        # --- Perform action based on decision ---
        if action == 's':
            print(f"Skipping generation for {description}")
            return None
        elif action == 'o':
            try:
                if os.path.isdir(output_path): shutil.rmtree(output_path)
                else: os.remove(output_path)
                print(f"Removed existing path: {rel_path}")
            except OSError as e:
                print(f"Error removing path {rel_path}: {e}")
                return None
        elif action == 'c':
            counter = 1
            base, ext = (os.path.splitext(output_path)) if not os.path.isdir(output_path) else (output_path, "")
            new_path = f"{base}_Copy{counter}{ext}"
            while os.path.exists(new_path): 
                counter += 1
                new_path = f"{base}_Copy{counter}{ext}"
            output_path = new_path
            print(f"Creating a copy: {os.path.relpath(output_path)}")
    
    # --- Generate file/dir ---
    generation_func(output_path, *gen_args)
    return output_path
