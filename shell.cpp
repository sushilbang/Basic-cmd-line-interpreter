#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <signal.h>

const int MAX_COMMAND_LEN = 1024;
const int MAX_ARGS = 64;
const std::string WHITESPACE = " \t\n\r\f\v";

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(WHITESPACE);
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(WHITESPACE);
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> split(const std::string& str, const std::string& delim = " ") {
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos - prev);
        token = trim(token);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

std::vector<char*> create_argv(const std::vector<std::string>& tokens) {
    std::vector<char*> argv;
    for (const auto& token : tokens) {
        argv.push_back(const_cast<char*>(token.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

void reap_background_processes() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        std::cout << "[Shell] Background process " << pid << " terminated." << std::endl;
    }
}

void execute_command(const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        return;
    }

    if (tokens[0] == "exit") {
        exit(EXIT_SUCCESS);
    } else if (tokens[0] == "cd") {
        if (tokens.size() < 2) {
            std::cerr << "cd: missing operand" << std::endl;
        } else {
            if (chdir(tokens[1].c_str()) != 0) {
                perror("cd failed");
            }
        }
        return;
    } else if (tokens[0] == "pwd") {
         char cwd[MAX_COMMAND_LEN];
         if (getcwd(cwd, sizeof(cwd)) != NULL) {
             std::cout << cwd << std::endl;
         } else {
             perror("pwd failed");
         }
         return;
    }

    std::vector<std::string> current_command_tokens;
    std::string input_file;
    std::string output_file;
    bool background = false;
    bool pipe_present = false;
    int pipe_token_index = -1;

    std::vector<std::string> final_tokens;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "&" && i == tokens.size() - 1) {
            background = true;
        } else if (tokens[i] == "<" && i + 1 < tokens.size()) {
            input_file = tokens[i + 1];
            i++;
        } else if (tokens[i] == ">" && i + 1 < tokens.size()) {
            output_file = tokens[i + 1];
            i++;
        } else if (tokens[i] == "|" && !pipe_present) {
            pipe_present = true;
            pipe_token_index = i;
            current_command_tokens = final_tokens;
            final_tokens.clear();
        } else {
            final_tokens.push_back(tokens[i]);
        }
    }
     if (!pipe_present) {
        current_command_tokens = final_tokens;
     }

    int pipefd[2];
    if (pipe_present) {
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            return;
        }
    }

    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork failed");
        if (pipe_present) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        return;
    }

    if (pid1 == 0) {
        if (!input_file.empty()) {
            int fd_in = open(input_file.c_str(), O_RDONLY);
            if (fd_in < 0) {
                perror(("Failed to open input file: " + input_file).c_str());
                exit(EXIT_FAILURE);
            }
            if (dup2(fd_in, STDIN_FILENO) < 0) {
                perror("dup2 input redirection failed");
                close(fd_in);
                exit(EXIT_FAILURE);
            }
            close(fd_in);
        }

        if (pipe_present) {
            close(pipefd[0]);
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                perror("dup2 pipe output redirection failed");
                close(pipefd[1]);
                exit(EXIT_FAILURE);
            }
            close(pipefd[1]);
        } else if (!output_file.empty()) {
            int fd_out = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644); // Create/Truncate file
            if (fd_out < 0) {
                perror(("Failed to open output file: " + output_file).c_str());
                exit(EXIT_FAILURE);
            }
            if (dup2(fd_out, STDOUT_FILENO) < 0) {
                perror("dup2 output redirection failed");
                close(fd_out);
                exit(EXIT_FAILURE);
            }
            close(fd_out);
        }

        std::vector<char*> argv = create_argv(current_command_tokens);

        execvp(argv[0], argv.data());

        perror(("execvp failed for command: " + current_command_tokens[0]).c_str());
        exit(EXIT_FAILURE);

    } else {
        pid_t pid2 = -1;

        if (pipe_present) {
            pid2 = fork();
            if (pid2 < 0) {
                perror("fork for second command failed");
                 waitpid(pid1, nullptr, 0);
                 close(pipefd[0]);
                 close(pipefd[1]);
                 return;
            }

            if (pid2 == 0) {
                close(pipefd[1]);

                if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                    perror("dup2 pipe input redirection failed");
                    close(pipefd[0]);
                    exit(EXIT_FAILURE);
                }
                close(pipefd[0]);

                 if (!output_file.empty()) {
                    int fd_out = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out < 0) {
                        perror(("Failed to open output file for second command: " + output_file).c_str());
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(fd_out, STDOUT_FILENO) < 0) {
                        perror("dup2 output redirection failed for second command");
                        close(fd_out);
                        exit(EXIT_FAILURE);
                    }
                    close(fd_out);
                }

                std::vector<char*> argv = create_argv(final_tokens);

                execvp(argv[0], argv.data());
                perror(("execvp failed for command: " + final_tokens[0]).c_str());
                exit(EXIT_FAILURE);

            } else {
               close(pipefd[0]);
               close(pipefd[1]);
            }
        }

        if (!background) {
             int status1, status2;
            waitpid(pid1, &status1, 0);
            if (pipe_present && pid2 > 0) {
                waitpid(pid2, &status2, 0);
            }
        } else {
             std::cout << "[Shell] Background process started with PID: " << pid1 << std::endl;
             if (pipe_present && pid2 > 0) {
                 std::cout << "[Shell] Background process started with PID: " << pid2 << std::endl;
             }
        }
    }
}


int main() {
    std::string line;
    std::string prompt = "myshell> ";

    while (true) {
        reap_background_processes();

        std::cout << prompt;
        std::flush(std::cout);

        if (!std::getline(std::cin, line)) {
            std::cout << std::endl << "Exiting shell." << std::endl;
            break;
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> tokens = split(line);

        execute_command(tokens);
    }

    return EXIT_SUCCESS;
}