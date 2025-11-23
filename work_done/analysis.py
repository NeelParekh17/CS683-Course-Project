import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import os

# --- Create output directory for plots ---
output_dir = "correlation_plots"
os.makedirs(output_dir, exist_ok=True)

# --- Define datasets ---
files = [
    ("Base Configuration", "Base.csv"),
    ("Change 1", "Change_1.csv"),
    ("Change 2", "Change_2.csv")
]

# --- Process each dataset ---
for title, filename in files:
    print(f"\nProcessing {filename} ...")

    # Try both CSV formats (comma or tab-separated)
    try:
        df = pd.read_csv(filename)
    except:
        df = pd.read_csv(filename, sep='\t')

    # Drop non-numeric and empty columns
    df = df.drop(columns=["filename"], errors='ignore')
    df = df.dropna(axis=1, how='all')

    # Skip empty or invalid DataFrames
    if df.empty:
        print(f"⚠️ {filename} is empty or invalid, skipping.")
        continue

    # --- Compute correlation ---
    corr = df.corr(method='pearson')

    # --- Create the correlation plot ---
    plt.figure(figsize=(10, 8))
    sns.heatmap(
        corr, annot=True, fmt=".2f", cmap="coolwarm",
        center=0, square=True, cbar_kws={'shrink': 0.8}
    )
    plt.title(f"Correlation Matrix - {title}")
    plt.tight_layout()

    # --- Save the figure ---
    save_path = os.path.join(output_dir, f"corr_{os.path.splitext(filename)[0]}.png")
    plt.savefig(save_path, dpi=300)
    plt.close()

    print(f"✅ Saved correlation plot to: {save_path}")

    # --- Print correlation with IPC (if column exists) ---
    if "IPC" in df.columns:
        ipc_corr = corr["IPC"].sort_values(ascending=False)
        print("\nCorrelation of each metric with IPC:")
        print(ipc_corr.round(3))

print("\n🎉 All plots have been generated and saved in the 'correlation_plots/' folder.")
