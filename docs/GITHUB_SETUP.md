# GitHub setup notes

This folder was prepared to be a normal Git/GitHub repository.

## First-time local Git setup

From the project folder:

```powershell
git init
git add .
git commit -m "Initial fish finder stabilizer proof of concept"
```

If Git asks who you are:

```powershell
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```

## Create the GitHub repo

Create an empty repository on GitHub. Do not add a README there if this local folder already has one.

Then connect this folder to it:

```powershell
git branch -M main
git remote add origin https://github.com/YOUR-USER/YOUR-REPO.git
git push -u origin main
```

If you prefer SSH:

```powershell
git remote add origin git@github.com:YOUR-USER/YOUR-REPO.git
git push -u origin main
```

## What should be committed

Commit:

- Python source files
- Arduino sketches
- README and docs
- `requirements.txt`

Do not commit:

- `.venv/`
- `.idea/`
- `__pycache__/`
- CSV telemetry logs unless one is intentionally added as a sample
- personal local paths or screenshots unless intentionally documenting a bug

## Recommended future workflow

Before changing firmware or GUI code:

```powershell
git status
git pull
```

After a working change:

```powershell
git add .
git commit -m "Describe the working change"
git push
```

For hardware-debug sessions, save CSV logs outside the repo or in an ignored `logs/` folder.

