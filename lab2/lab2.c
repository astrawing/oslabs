#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>  

typedef struct {
    size_t rows;
    size_t cols;
    int *data;
} Matrix;

typedef struct {
    Matrix *input;
    Matrix *output;

    size_t filter_size;
    size_t iterations;

    size_t first_row;
    size_t last_row;

    int *window;

    pthread_barrier_t *barrier;
} ThreadArgs;

static Matrix matrix_alloc(size_t rows, size_t cols)
{
    Matrix m;
    m.rows = rows;
    m.cols = cols;
    if (cols != 0 && rows > SIZE_MAX / cols / sizeof(int)) {
        fprintf(stderr, "matrix size overflow\n");
        exit(EXIT_FAILURE);
    }
    m.data = malloc(rows * cols * sizeof(int));
    if (!m.data) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }
    return m;
}

static void matrix_free(Matrix *m)
{
    m->cols = 0;
    m->rows = 0;
    free(m->data);
    m->data = NULL;
}

static Matrix matrix_read(FILE *file)
{
    if (file == NULL) {
        fprintf(stderr, "NULL path\n");
        exit(EXIT_FAILURE);
    }
    size_t rows;
    size_t cols;

    if (fscanf(file, "%zu %zu", &rows, &cols) != 2) {
        fprintf(stderr, "failed to read matrix dimensions\n");
        exit(EXIT_FAILURE);
    }

    Matrix matrix = matrix_alloc(rows, cols);

    for (size_t i = 0; i < rows * cols; ++i) {
        if (fscanf(file, "%d", &matrix.data[i]) != 1) {
            fprintf(stderr, "failed to read matrix element\n");
            matrix_free(&matrix);
            exit(EXIT_FAILURE);
        }
    }

    return matrix;
}

static int compare_ints(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

static int is_out_of_bounds(const Matrix *matrix, long row, long col)
{
    return (row < 0 || row >= (long)matrix->rows ||
            col < 0 || col >= (long)matrix->cols);
}

static void apply_median_filter_row(
    const Matrix *input,
    Matrix *output,
    size_t row,
    size_t filter_size,
    int *window)
{
    size_t radius = filter_size / 2;
    size_t window_size = filter_size * filter_size;

    for (size_t col = 0; col < input->cols; ++col) {
        size_t count = 0;

        for (long di = -(long)radius; di <= (long)radius; ++di) {
            long input_row = (long)row + di;

            for (long dj = -(long)radius; dj <= (long)radius; ++dj) {
                long input_col = (long)col + dj;

                if (is_out_of_bounds(input, input_row, input_col)) {
                    window[count++] = 0;
                } else {
                    window[count++] = input->data[input_row * input->cols + input_col];
                }
            }
        }

        qsort(window, window_size, sizeof *window, compare_ints);

        output->data[row * output->cols + col] = window[window_size / 2];
    }
}

static void matrix_filter_sequential(
    const Matrix *input,
    Matrix *output,
    size_t filter_size)
{
    size_t window_size = filter_size * filter_size;

    int *window = malloc(window_size * sizeof *window);
    if (window == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }

    for (size_t row = 0; row < input->rows; ++row) {
        apply_median_filter_row(input, output, row, filter_size, window);
    }

    free(window);
}

static void *filter_thread(void *arg)
{
    ThreadArgs *args = arg;

    for (size_t k = 0; k < args->iterations; ++k) {
        for (size_t row = args->first_row; row < args->last_row; ++row) {
            apply_median_filter_row(args->input, args->output, row, args->filter_size, args->window);
        }

        pthread_barrier_wait(args->barrier);

        Matrix *temp = args->input;
        args->input = args->output;
        args->output = temp;
    }

    return NULL;
}

static void matrix_filter_parallel(
    Matrix *input,
    Matrix *output,
    size_t filter_size,
    size_t iterations,
    size_t thread_count)
{
    if (thread_count > input->rows) {
        thread_count = input->rows;
    }
    if (thread_count == 0) {
        return;  
    }

    pthread_t *threads = malloc(thread_count * sizeof *threads);
    ThreadArgs *args = malloc(thread_count * sizeof *args);

    if (threads == NULL || args == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }

    pthread_barrier_t barrier;

    if (pthread_barrier_init(&barrier, NULL, (unsigned)thread_count) != 0) {
        fprintf(stderr, "failed to initialize barrier\n");
        exit(EXIT_FAILURE);
    }

    size_t base_rows = input->rows / thread_count;
    size_t extra_rows = input->rows % thread_count;

    size_t window_size = filter_size * filter_size;

    for (size_t i = 0; i < thread_count; ++i) {
        args[i].input = input;
        args[i].output = output;

        args[i].filter_size = filter_size;
        args[i].iterations = iterations;

        args[i].first_row = i * base_rows + (i < extra_rows ? i : extra_rows);
        args[i].last_row = args[i].first_row + base_rows + (i < extra_rows ? 1 : 0);

        args[i].window = malloc(window_size * sizeof *args[i].window);

        if (args[i].window == NULL) {
            fprintf(stderr, "windows malloc failed\n");
            exit(EXIT_FAILURE);
        }

        args[i].barrier = &barrier;

        if (pthread_create(&threads[i], NULL, filter_thread, &args[i]) != 0) {
            fprintf(stderr, "failed to create thread %zu\n", i);
            exit(EXIT_FAILURE);
        }
    }

    for (size_t i = 0; i < thread_count; ++i) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);

    for (size_t i = 0; i < thread_count; ++i) {
        free(args[i].window);
    }

    free(args);
    free(threads);
}

static void matrix_write(FILE *file, const Matrix *matrix)
{
    fprintf(file, "%zu %zu\n", matrix->rows, matrix->cols);

    for (size_t i = 0; i < matrix->rows; ++i) {
        for (size_t j = 0; j < matrix->cols; ++j) {
            fprintf(file, "%d", matrix->data[i * matrix->cols + j]);

            if (j + 1 < matrix->cols) {
                fputc(' ', file);
            }
        }

        fputc('\n', file);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <input_file> <output_file> " "<thread_count> <iterations> <filter_size>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];

    char *endptr;

    unsigned long thread_count_value = strtoul(argv[3], &endptr, 10);

    if (*argv[3] == '\0' ||
        *endptr != '\0' ||
        thread_count_value == 0) {

        fprintf(stderr, "invalid thread count\n");
        return EXIT_FAILURE;
    }

    unsigned long iterations_value = strtoul(argv[4], &endptr, 10);

    if (*argv[4] == '\0' ||
        *endptr != '\0') {

        fprintf(stderr, "invalid iterations count\n");
        return EXIT_FAILURE;
    }

    unsigned long filter_size_value = strtoul(argv[5], &endptr, 10);

    if (*argv[5] == '\0' ||
        *endptr != '\0' ||
        filter_size_value == 0 ||
        filter_size_value % 2 == 0 ||
        filter_size_value > SIZE_MAX / filter_size_value / sizeof(int)) {

        fprintf(stderr, "invalid filter size (must be a positive odd number)\n");
        return EXIT_FAILURE;
    }

    size_t thread_count = (size_t)thread_count_value;
    size_t iterations = (size_t)iterations_value;
    size_t filter_size = (size_t)filter_size_value;

    FILE *input_file = fopen(input_filename, "r");
    if (input_file == NULL) {
        perror("failed to open input file");
        return EXIT_FAILURE;
    }

    Matrix input = matrix_read(input_file);
    fclose(input_file);

    Matrix output = matrix_alloc(input.rows, input.cols);

    const Matrix *result;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    if (thread_count == 1) {
        for (size_t k = 0; k < iterations; ++k) {
            matrix_filter_sequential(&input, &output, filter_size);
            Matrix tmp = input;
            input = output;
            output = tmp;
        }
        result = &input;
    } else {
        matrix_filter_parallel(&input, &output, filter_size, iterations, thread_count);
        result = (iterations % 2 == 0) ? &input : &output;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    printf("%.3f ms\n", ms);

    FILE *output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        perror("failed to open output file");

        matrix_free(&input);
        matrix_free(&output);

        return EXIT_FAILURE;
    }

    matrix_write(output_file, result);

    fclose(output_file);

    matrix_free(&input);
    matrix_free(&output);

    return EXIT_SUCCESS;
}