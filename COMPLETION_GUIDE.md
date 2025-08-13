# POSIX Shell Scripts with Automatic Bash Completion

This document demonstrates how to write shell scripts that automatically provide bash completion without requiring separate completion files or manual sourcing.

## Automatic Self-Registering Completion

This approach makes the script automatically register its own completion when bash encounters it:

```bash
#!/bin/bash

# Define available commands
COMMANDS="start stop restart status help"

# Auto-register completion when script is sourced or executed in completion context
if [[ "${BASH_SOURCE[0]}" != "${0}" ]] || [[ "${COMP_WORDS}" ]]; then
    # We're being sourced for completion or called by bash completion
    _myservice_completion() {
        local cur="${COMP_WORDS[COMP_CWORD]}"
        COMPREPLY=( $(compgen -W "${COMMANDS}" -- "${cur}") )
    }
    complete -F _myservice_completion "${BASH_SOURCE[0]##*/}"
    complete -F _myservice_completion myservice
    return 2>/dev/null || exit 0
fi

# Main script logic
main() {
    case "$1" in
        start) echo "Starting service..." ;;
        stop) echo "Stopping service..." ;;
        restart) echo "Restarting service..." ;;
        status) echo "Service status..." ;;
        help) echo "Usage: $0 {start|stop|restart|status|help}" ;;
        *) echo "Unknown command: $1" ;;
    esac
}

# Execute when run directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
```

## How It Works

1. **Detection**: The script detects if it's being sourced (`${BASH_SOURCE[0]}" != "${0}"`) or if it's in a completion context (`${COMP_WORDS}`)

2. **Auto-registration**: When detected, it registers its completion function and exits/returns

3. **Normal execution**: When run normally, it executes the main logic

## Advanced Example with Dynamic Commands

```bash
#!/bin/bash

# Function to get available commands (can be dynamic)
get_commands() {
    # Could read from config, parse help output, etc.
    echo "build test deploy clean init help"
}

# Auto-register completion
if [[ "${BASH_SOURCE[0]}" != "${0}" ]] || [[ "${COMP_WORDS}" ]]; then
    _dynamic_completion() {
        local cur prev
        cur="${COMP_WORDS[COMP_CWORD]}"
        prev="${COMP_WORDS[COMP_CWORD-1]}"
        
        case "$prev" in
            deploy)
                # Context-aware completion for deploy environments
                COMPREPLY=( $(compgen -W "staging production dev" -- "${cur}") )
                ;;
            *)
                # Get commands dynamically
                local commands=$(get_commands)
                COMPREPLY=( $(compgen -W "${commands}" -- "${cur}") )
                ;;
        esac
    }
    complete -F _dynamic_completion "${BASH_SOURCE[0]##*/}"
    return 2>/dev/null || exit 0
fi

# Main script logic
case "$1" in
    build) echo "Building..." ;;
    test) echo "Testing..." ;;
    deploy) echo "Deploying to $2..." ;;
    clean) echo "Cleaning..." ;;
    init) echo "Initializing..." ;;
    help) echo "Available commands: $(get_commands)" ;;
    *) echo "Unknown command. Use 'help' for available commands." ;;
esac
```

## Installation and Usage

1. **Make script executable:**
   ```bash
   chmod +x myscript
   ```

2. **That's it!** No additional files or sourcing needed. Bash will automatically provide completion when you use tab.

## Key Benefits

- ✅ **Zero setup** - No separate completion files
- ✅ **Self-contained** - Everything in one script
- ✅ **Automatic** - No manual sourcing required
- ✅ **Dynamic** - Can generate completions on-the-fly
- ✅ **Context-aware** - Can provide different completions based on previous arguments

## Best Practices

1. **Use meaningful command names** - easier to remember and complete
2. **Keep completion logic simple** - avoid complex operations in completion functions
3. **Handle edge cases** - empty arguments, unknown commands, etc.
4. **Test thoroughly** - ensure completion works and doesn't interfere with normal execution
5. **Consider performance** - completion functions should be fast

## Advanced Features

### File Completion
```bash
_file_completion() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    case "${COMP_WORDS[1]}" in
        edit|view)
            # Complete with filenames
            COMPREPLY=( $(compgen -f -- "${cur}") )
            ;;
        *)
            COMPREPLY=( $(compgen -W "edit view build test" -- "${cur}") )
            ;;
    esac
}
```

### Command Chaining
```bash
_chained_completion() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local cmd="${COMP_WORDS[1]}"
    
    if [[ ${COMP_CWORD} -eq 1 ]]; then
        # First argument - complete with main commands
        COMPREPLY=( $(compgen -W "docker git make npm" -- "${cur}") )
    elif [[ ${COMP_CWORD} -eq 2 ]]; then
        # Second argument - complete based on first command
        case "$cmd" in
            docker) COMPREPLY=( $(compgen -W "build run push pull" -- "${cur}") ) ;;
            git) COMPREPLY=( $(compgen -W "add commit push pull status" -- "${cur}") ) ;;
            make) COMPREPLY=( $(compgen -W "build test clean install" -- "${cur}") ) ;;
            npm) COMPREPLY=( $(compgen -W "install build test start" -- "${cur}") ) ;;
        esac
    fi
}
```

This approach provides the cleanest user experience - your script just works with tab completion automatically, without any setup required.
