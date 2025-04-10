# GitHub Copilot Instructions

This document provides guidance for GitHub Copilot to generate code and terminal commands that align with this project's standards. These instructions help ensure consistency, readability, security, and maintainability across all contributions.

---

## 🧠 General Philosophy

- Favor **clarity and maintainability** over clever or abstract solutions.
- **Don’t guess** — when Copilot is unsure, it should reference `/explain`, `/terminal`, or `/problems` for clues before continuing.
- Keep logic aligned with the **existing structure and idioms** of the codebase.
- Avoid “magic” or unexplained solutions; strive for transparency in logic.

---

## 🧰 Use Copilot Chat Commands Actively

Copilot should use its tools to write better code. Commands that must be used regularly:

- **/explain** — Before modifying complex or unfamiliar code, explain it.
- **/fix** — Apply only after investigating root cause via terminal and/or Problems tab.
- **/tests** — Generate or update tests when adding/modifying core functionality.
- **/problems** — Review issues in the Problems panel before making assumptions.
- **/terminal** — Check recent output for errors, crashes, or log messages before proposing fixes.
- **/optimize** — Only after baseline functionality is confirmed and tested.

---

## 📁 Project Structure & Layout

Follow consistent structure across projects (backend, frontend, full-stack):

### Backend (Python / Node.js)

- Structure code into `routes/`, `controllers/`, `models/`, `services/`, `utils/`
- Keep `app.py`, `main.py`, or `index.js` minimal — delegate to modules
- Use environment variables (via `dotenv`) — no hardcoded secrets
- Use dependency pinning (`requirements.txt` or `package-lock.json`)

### Frontend (React / JS / TS)

- Use `components/`, `hooks/`, `store/`, `styles/`, `pages/`, `lib/` folders
- Favor functional components and hooks over class components
- Keep logic, UI, and styles cleanly separated

---

## ✨ Code Style & Practices

### Python

- Follow **PEP 8**
- Use `black` for formatting and `flake8` or `pylint` for linting
- Include type hints where useful
- Use docstrings on public classes and functions

### JavaScript / TypeScript

- Use camelCase for vars/functions, PascalCase for components
- Use Prettier and ESLint (Airbnb or Standard config)
- Prefer async/await over callbacks or chained `.then()`
- Use `const` and `let` appropriately (avoid `var`)

---

## 🧪 Testing

- Testing is **not optional** — new logic must include or extend tests
- JS: use `jest` or `vitest` with `.test.js` or `__tests__/`
- Python: use `pytest` with `tests/` folder
- Include:
  - Expected inputs and outputs
  - Edge cases
  - Error handling

---

## ⚙️ Environment & Deployment

- Use `.env` files + `dotenv` for all sensitive config
- Never commit `.env` files or secrets
- Code should work in both local dev and cloud environments (e.g., Vercel, Netlify, Azure)
- Avoid platform-specific logic unless absolutely necessary

---

## 🔐 Security

- Always sanitize and validate user input
- Never expose API keys, secrets, or credentials
- Escape output in templates (JSX, Jinja, etc.)
- Avoid `eval`, `exec`, or any dynamic code execution
- Keep dependencies updated to avoid vulnerabilities

---

## ⚡ Performance & Efficiency

- Don’t optimize prematurely — prefer clarity first
- Use efficient data structures, and avoid redundant computation
- For async operations, always handle errors with try/catch or `.catch()`
- Use pagination or lazy loading for large data sets

---

## 🧹 Clean Code Rules

- Remove unused variables, imports, and commented-out code
- Use descriptive variable and function names
- Group related logic together in modules
- Use comments sparingly — only to explain non-obvious decisions

---

## 🖥️ Terminal Command Guidelines

To ensure consistency and efficiency in terminal operations, GitHub Copilot should adhere to the following guidelines when generating or suggesting terminal commands:

- **Package Management**: Prefer `pnpm` over `npm` for package management tasks due to its speed and disk space efficiency. For example, use `pnpm install` instead of `npm install`.

- **Path Specifications**: Use absolute paths when specifying file or directory locations to avoid ambiguity and ensure commands function correctly regardless of the current working directory.

- **Command Verification**: Before executing suggested commands, especially those that modify or delete data, verify their accuracy and impact to prevent unintended consequences.

- **Alias Usage**: Utilize shell aliases to streamline frequent commands and reduce the potential for errors. For example, create an alias for common directory navigation: `alias proj='cd /absolute/path/to/project'`.

- **Environment Consistency**: Ensure that the terminal environment is consistent across development setups by using tools like `.env` files to manage environment variables securely.

- **Documentation and Comments**: When writing shell scripts or complex command sequences, include comments to explain the purpose and functionality of each part, enhancing maintainability and collaboration.

## 📎 Use of Example Code and Repositories

When GitHub Copilot is provided with links to example code (e.g., GitHub repositories, gists, or code snippets from documentation or blog posts), it must:

- **Analyze patterns** and structure from the referenced example before generating new code.
- **Replicate idioms, architecture, and naming conventions** seen in the example when extending or building similar logic.
- **Respect file and folder layouts**, and create new code in harmony with the example's organization.
- Use examples as **primary references** for unfamiliar domains or frameworks.
- Avoid diverging from example patterns unless there's a clear and justifiable improvement.

> 🔗 When given links to `github.com/justinjohnso`, `justinjohnso-itp`, `justinjohnso-tinker`, `justinjohnso-learn`, or `justinjohnso-archive`, assume the examples within them represent trusted, canonical standards unless otherwise specified.

## 📝 Writing Blog Posts

When asked to generate a blog post or write-up, GitHub Copilot should follow these principles to stay aligned with the tone and expectations of the author’s personal blog:

### ✅ Voice & Style

- **Tone**: Use a **casual and conversational voice**. Avoid sounding like a brand or influencer. It's fine to be direct, dryly funny, or a little nerdy.
- **Audience Assumption**: Assume the reader has general technical fluency — no need to explain what HTML or APIs are, for example.
- **First-person perspective**: It's okay to use "I" sparingly to reflect the author's process or decisions — especially when talking through problems or why something was chosen.
- **Avoid**: Clickbait titles, over-explaining, or sounding overly enthusiastic about basic things.

### 🧱 Structure & Flow

- Posts don’t need to follow a rigid format — just be logically ordered:
  - What was attempted or built
  - What decisions were made and why
  - Snippets or visuals when useful
  - Links to tools, repos, or docs
  - Any open questions or future improvements

### 🔗 Content & References

- Link to external docs or GitHub repos instead of copying long code blocks unless the snippet is central to the point.
- Prefer practical examples and brief explanations over tutorials.
- Don’t restate what’s already obvious from referenced material — assume readers will check the links if curious.

### 📌 Example Style Notes

- It’s fine to start posts mid-thought, like:
  > “This started as a 2-hour experiment and got out of hand.”

- Use bullet points or code blocks when needed, but don’t overformat.
- Be honest about what worked and what didn’t — no need to polish everything.

> ✍️ When in doubt, match the tone and pacing of posts on [Justin’s ITP Blog](https://justin-itp.notion.site/06090509687040a8a7381153743e3e5b?v=1139127f465d80069d80000c68e0824f)

---

## ✅ Final Copilot Behavior Expectations

Before submitting generated code, Copilot should:

- Use `/problems` to confirm there are no warnings or errors
- Use `/terminal` to check logs or output from prior runs
- Use `/tests` to write or update test coverage
- Use `/explain` when the code being edited isn't clear
- Write idiomatic, clean, and consistent code based on project norms

---

> Place this file in `.github/copilot-instructions.md` to guide GitHub Copilot's behavior across all repositories.