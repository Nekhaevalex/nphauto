package main

import (
	"encoding/csv"
	"flag"
	"fmt"
	"log"
	"os"
	"strings"
	"time"

	"github.com/Nekhaevalex/nphauto/sensors/moisture"
)

type params struct {
	path     string
	interval time.Duration
	quantity time.Duration
}

var p params

func init() {
	// Parsing arguments
	tablePath := flag.String("table", "data.csv", "path to table csv file")
	frequency := flag.Int("int", 12, "tick time interval numeric part")
	sQuanity := flag.String("qty", "h", "tick time interval quantity (h/m/s)")
	flag.Parse()

	var quantity time.Duration
	switch strings.TrimSpace(*sQuanity) {
	case "h":
		quantity = time.Hour
	case "m":
		quantity = time.Minute
	case "s":
		quantity = time.Second
	default:
		log.Fatalf("wrong quantity '%s', expected h/m/s", *sQuanity)
	}

	p = params{
		path:     *tablePath,
		interval: time.Duration(*frequency),
		quantity: quantity,
	}
}

func main() {
	// Setting up thread ticker
	ticker := time.NewTicker(p.interval * p.quantity)
	// Thread done channel
	done := make(chan bool)

	// Setting up moisture sensor connection
	ms, err := moisture.NewMoistureSensor("DracaenaMoisture")
	if err != nil {
		log.Fatal(err)
	}

	// Connecting
	if err = ms.Connect(); err != nil {
		log.Fatal(err)
	}

	defer func() {
		if err := ms.Disconnect(); err != nil {
			log.Fatal(err)
		}
	}()

	// Setting up CSV file
	file, err := os.OpenFile(p.path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0660)
	if err != nil {
		log.Fatal(err)
	}

	defer func() {
		if err := file.Close(); err != nil {
			log.Fatal(err)
		}
	}()

	var data float64              // Variable for storing moisture value
	writer := csv.NewWriter(file) // Connecting writer to CSV file

	defer func() {
		writer.Flush()
		if err := writer.Error(); err != nil {
			log.Fatal(err)
		}
	}()

	// Reading loop
	for {
		select {
		case <-ticker.C:
			// On tick
			err = ms.Read(&data)
			if err != nil {
				log.Fatal(err)
			}

			now := fmt.Sprintf("%d", time.Now().Unix())
			val := fmt.Sprintf("%f", data)

			fmt.Printf("%s: %f\n", time.Now().String(), data)

			if err = writer.Write([]string{now, val}); err != nil {
				log.Fatal(err)
			}

			writer.Flush()
			if err = writer.Error(); err != nil {
				log.Fatal(err)
			}
		case <-done:
			// On finish
			return
		}
	}
}
