from dash import Dash, html, dcc, callback, Output, Input, dash_table
import plotly.express as px
import pandas as pd
import argparse as ap
import time
import datetime

version = "0.1"

df_path: str

df: pd.DataFrame

app = Dash()

def getLayout():
    try:
        df = pd.read_csv(df_path, header=None, names = ["timestamp", "value"])
    except:
        df = pd.DataFrame({"timestamp":[], "value": []})

    df = df.assign(
        timestamp = pd.to_datetime(df["timestamp"], unit='s'),
        value = df["value"] * 100
    )\
        .set_index("timestamp")

    last_row = df.tail(1)

    try:
        last_m = last_row.index[0]
        last_v = last_row.values[0][0]
    except:
        last_m = 0
        last_v = 0

    return html.Div([
        html.H1(children="NPH Soil Moisture", style={'textAlign':'left'}),
        html.Div([
            html.P(f"Last measurement at {last_m}"),
            html.P(f"Last measurement value: {last_v}%")
        ]),
        dash_table.DataTable(data=df.reset_index().to_dict('records'), page_size=10),
        dcc.Graph(figure= px.line(df, y = "value", title="Soil Moisture Historical Data", labels = ["Time", "Soil Moisture (%)"]))
    ])

app.layout = getLayout

# @callback(
#     Input(),
#     Output('graph-content', 'figure')
# )
# def update_graph(value):
#     return px.line(df, x = "timestamp", y = "value")

if __name__ == "__main__":
    parser = ap.ArgumentParser(
        prog="dracaena_monitor",
        description="Web server for NPH Automation Soil Moisture Sensor (dracaena)",
        epilog=f"version {version}"
    )
    parser.add_argument("path", help="path to csv file produces by BLE server")
    args = parser.parse_args()
    df_path = args.path



    app.run(debug=True)